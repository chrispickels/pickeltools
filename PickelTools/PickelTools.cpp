#include "pch.h"
#include "PickelTools.h"

#include <sstream>
#include <format>

#include "bakkesmod/wrappers/cvarmanagerwrapper.h"
#include "IMGUI/imgui.h"

BAKKESMOD_PLUGIN(PickelTools, "PickelTools", plugin_version, PLUGINTYPE_FREEPLAY)

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

namespace {

template <typename... Args>
void LOG(std::string_view format_str, Args&&... args)
{
	_globalCvarManager->log(std::vformat(format_str, std::make_format_args(std::forward<Args>(args)...)));
}

bool isNearlyEqual(float a, float b) {
  constexpr int kFactor = 2;
  const float min_a = a - (a - std::nextafter(a, std::numeric_limits<float>::lowest())) * kFactor;
  const float max_a = a + (std::nextafter(a, std::numeric_limits<float>::max()) - a) * kFactor;
  return min_a <= b && max_a >= b;
}

void clearPlaylists(MatchmakingWrapper& mm) {
  constexpr Playlist playlists[] = {
      Playlist::CASUAL_STANDARD, Playlist::CASUAL_DOUBLES,
      Playlist::CASUAL_DUELS,    Playlist::CASUAL_CHAOS,
      Playlist::RANKED_STANDARD, Playlist::RANKED_DOUBLES,
      Playlist::RANKED_DUELS,    Playlist::AUTO_TOURNAMENT,
      Playlist::EXTRAS_RUMBLE,   Playlist::EXTRAS_DROPSHOT,
      Playlist::EXTRAS_HOOPS,    Playlist::EXTRAS_SNOWDAY};
	for (int i = 0; i < IM_ARRAYSIZE(playlists); ++i) {
		mm.SetPlaylistSelection(playlists[i], false);
	}
}

}  // namespace

void PickelTools::onLoad() {
	_globalCvarManager = cvarManager;

	cvarManager->registerCvar(enabledCvarName, "1", "Determines whether PickelTools is enabled.").addOnValueChanged(std::bind(&PickelTools::pluginEnabledChanged, this));
	cvarManager->registerCvar(trainingMapCvarName, "EuroStadium_Night_P", "Determines the map that will launch for training.");

	uniqueId = gameWrapper->GetUniqueID();
	LOG("Player's UniqueID is {}", uniqueId.GetIdString());

	ranks= buildNewRanks();
	LOG("Ranks initialized: {}", ranksToString(ranks));

	mmrNotifierToken = gameWrapper->GetMMRWrapper().RegisterMMRNotifier(
			[this](UniqueIDWrapper id) {
				onMmrUpdate(id);
			});

	pluginEnabledChanged();
}

void PickelTools::onUnload() {
	mmrNotifierToken.reset();
	awaitingFinalMmrUpdate = false;
}

void PickelTools::RenderSettings() {
	const char* items[] = { "Ranked Duel", "Ranked Doubles", "Ranked Standard" };
	static int selectedGameMode = 0;
	ImGui::ListBox("Game Mode", &selectedGameMode, items, IM_ARRAYSIZE(items), -1);
	switch (selectedGameMode) {
	case 0:
		gameMode = RankedDuel;
		break;
	case 1:
		gameMode = RankedDoubles;
		break;
	case 2:
		gameMode = RankedStandard;
		break;
	default:
		break;
	}

	static int numGames = 5;
	if (ImGui::Button("Five")) {
		numGames = 5;
	}
	ImGui::SameLine();
	if (ImGui::Button("Ten")) {
		numGames = 10;
	}
	ImGui::SameLine();
	if (ImGui::Button("Twenty")) {
		numGames = 20;
	}

	if (gamesRemaining > 0) {
		ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0 / 7.0f, 0.6f, 0.6f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0 / 7.0f, 0.7f, 0.7f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0 / 7.0f, 0.8f, 0.8f));

		std::string endLabel = std::format("End ({} games left)", gamesRemaining);
		if (ImGui::Button(endLabel.c_str())) {
			gameWrapper->Execute([this](GameWrapper* gw) {
				endSession();
			});
		}

		ImGui::PopStyleColor(3);
	} else {
		ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(2 / 7.0f, 0.6f, 0.6f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(2 / 7.0f, 0.7f, 0.7f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(2 / 7.0f, 0.8f, 0.8f));

		if (ImGui::Button("Start")) {
			gameWrapper->Execute([this](GameWrapper* gw) {
				if (gamesRemaining > 0) {
					endSession();
				}
				gamesRemaining = numGames;
				startSession();
			});
		}

		ImGui::PopStyleColor(3);
	}
	
	ImGui::SameLine();
	ImGui::InputInt("Number of Games", &numGames);
}

std::string PickelTools::GetPluginName() { return "PickelTools"; }

void PickelTools::SetImGuiContext(uintptr_t ctx) {
	ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(ctx));
}

// static
std::string PickelTools::ranksToString(const Ranks& ranks) {
	return std::format("rankedDuel={:.1f}, rankedDoubles={:.1f}, rankedStandard={:.1f}", ranks.rankedDuel, ranks.rankedDoubles, ranks.rankedStandard);
}

// static
#define ID_AND_NAME(x) case x: return #x
const char* PickelTools::modeToString(Mode mode) {
  switch (mode) {
		ID_AND_NAME(CasualDuel);
		ID_AND_NAME(CasualDoubles);
		ID_AND_NAME(CasualStandard);
		ID_AND_NAME(CasualChaos);
		ID_AND_NAME(Private);
		ID_AND_NAME(RankedDuel);
		ID_AND_NAME(RankedDoubles);
		ID_AND_NAME(RankedSoloStandard);
		ID_AND_NAME(RankedStandard);
		ID_AND_NAME(MutatorMashup);
		ID_AND_NAME(Tournament);
		ID_AND_NAME(RankedHoops);
		ID_AND_NAME(RankedRumble);
		ID_AND_NAME(RankedDropshot);
		ID_AND_NAME(RankedSnowday);
		ID_AND_NAME(GodBall);
		ID_AND_NAME(GodBallDoubles);
  }
}
#undef ID_AND_NAME

void PickelTools::pluginEnabledChanged() {
	const bool enabled = cvarManager->getCvar(enabledCvarName).getBoolValue();

	if (enabled) {
		if (!hooked) {
			hookMatchEnded();
		}
	}
	else {
		if (hooked) {
			unhookMatchEnded();
		}
	}
}

void PickelTools::queue(int retries) {
	LOG("queue() retries={} ...", retries);
	if (retries == 0) {
		LOG("Exhausted retries, giving up");
		return;
	}

	auto retry = [=] {
		constexpr float kQueueRetryWait = 0.1f;
		gameWrapper->SetTimeout([this, retries](GameWrapper* gw) {
			queue(retries - 1);
		}, kQueueRetryWait);
	};

	MatchmakingWrapper mm = gameWrapper->GetMatchmakingWrapper();
	if (mm.IsNull()) {
		LOG("MatchmakingWrapper is null");
		retry();
		return;
	}

	
	clearPlaylists(mm);
	switch (gameMode) {
	case RankedDuel:
		mm.SetPlaylistSelection(Playlist::RANKED_DUELS, true);
		break;
	case RankedDoubles:
		mm.SetPlaylistSelection(Playlist::RANKED_DOUBLES, true);
		break;
	case RankedStandard:
		mm.SetPlaylistSelection(Playlist::RANKED_STANDARD, true);
		break;
	default:
		LOG("Unknown game mode {}", gameMode);
		return;
	}
	mm.StartMatchmaking(PlaylistCategory::RANKED);

	if (!mm.IsSearching()) {
		LOG("Still not searching, try again...");
		retry();
		return;
	}

	LOG("Queued successfully!");
}

void PickelTools::startTraining() {
	std::stringstream s;
	std::string mapname = cvarManager->getCvar(trainingMapCvarName).getStringValue();
	s << "start " << mapname << "?Game=TAGame.GameInfo_Tutorial_TA?GameTags=Freeplay";

	auto game = gameWrapper->GetOnlineGame();
	if (!game.IsNull()) {
		if (game.GetbHasLeaveMatchPenalty()) return;
	}

	LOG("startTraining command='{}'", s.str());
	gameWrapper->ExecuteUnrealCommand(s.str());
}

void PickelTools::onMatchEnd(ServerWrapper server, void* params, std::string eventName) {
	const std::string matchGuid = server.GetMatchGUID();
	LOG("onMatchEnd for match={}", matchGuid);
	if (lastMatchGuid == matchGuid) {
		LOG("Already received onMatchEnd for match={}, ignoring...", matchGuid);
		return;
	}
	lastMatchGuid = matchGuid;

	if (gamesRemaining == 0) {
		LOG("No active session");
		return;
	}
	
	++gamesPlayed;
	--gamesRemaining;
	LOG("gamesPlayed={}, gamesRemaining={}", gamesPlayed, gamesRemaining);
	if (gamesRemaining == 0) {
		endSession();
		return;
	}

	if (!server.IsNull() && !server.GetPlaylist().IsNull()) {
		auto playlist = static_cast<Mode>(server.GetPlaylist().GetPlaylistId());
		if (playlist != RankedDuel && playlist != RankedDoubles && playlist != RankedStandard) {
			LOG("Unsupported playlist={}", modeToString(playlist));
			return;
		}
	}

	startTraining();
	queue();
}

void PickelTools::onPenaltyChanged(ServerWrapper server, void* params, std::string eventName) {
	if (server.GetbHasLeaveMatchPenalty()) return;

	// Match leave penalty has been lifted. Either:
	// 1) The final goal was scored, and players are now free to leave.
	// 2) Someone has abandoned the game, but it is not yet over.
	//
	// We want to instantly requeue in the event of (1), but in the event of (2), we can just ignore
	// the event completely as this is an exceptional situation. We detect (2) by checking if the game
	// should be over or not.

	const bool bForfeit = server.GetbForfeit();
	LOG("Forfeit={}", bForfeit);
	if (bForfeit) {
		LOG("Game is forfeit, ready to leave");
		onMatchEnd(server, params, eventName);
		return;
	}
	
	ArrayWrapper<TeamWrapper> teams = server.GetTeams();
	LOG("Number of teams: {}", teams.Count());
	if (teams.Count() != 2) return;

	const bool bOvertime = server.GetbOverTime();
	LOG("Overtime={}", bOvertime);
	if (bOvertime) {
		LOG("Score is {} to {}", teams.Get(0).GetScore(), teams.Get(1).GetScore());
		if (teams.Get(0).GetScore() == teams.Get(1).GetScore()) return;
		
		LOG("Overtime looks done, ready to leave");
		onMatchEnd(server, params, eventName);
		return;
	}

	LOG("Game time remaining: {}", server.GetGameTimeRemaining());
	if (server.GetGameTimeRemaining() > 0.f) return;

	LOG("Score is {} to {}", teams.Get(0).GetScore(), teams.Get(1).GetScore());
	if (teams.Get(0).GetScore() == teams.Get(1).GetScore()) return;
	
	LOG("Game looks done, ready to leave");
	onMatchEnd(server, params, eventName);
}

void PickelTools::startSession() {
	LOG("Start session, gamesRemaining={}", gamesRemaining);

	awaitingFinalMmrUpdate = false;
	gamesPlayed = 0;

	if (gamesRemaining == 0) return;

	MatchmakingWrapper mm = gameWrapper->GetMatchmakingWrapper();
	if (mm.IsNull()) {
		gamesRemaining = 0;
		return;
	}

	LOG("Start session with ranks {}", ranksToString(ranks));
	startSessionRanks = ranks;

	queue();
	if (!gameWrapper->IsInFreeplay() &&
			!gameWrapper->IsInReplay() &&
			!gameWrapper->IsInCustomTraining()) {
		startTraining();
	}

	cvarManager->executeCommand("closemenu settings");
}

void PickelTools::endSession() {
	LOG("End session");

	MatchmakingWrapper mm = gameWrapper->GetMatchmakingWrapper();
	if (!mm.IsNull() && mm.IsSearching()) {
		LOG("Stop matchmaking");
		mm.CancelMatchmaking();
	}

	gamesRemaining = 0;
	if (gamesPlayed == 0) return;

	awaitingFinalMmrUpdate = true;
}

PickelTools::Ranks PickelTools::buildNewRanks() {
	Ranks newRanks;
	newRanks.rankedDuel = gameWrapper->GetMMRWrapper().GetPlayerMMR(uniqueId, RankedDuel);
	newRanks.rankedDoubles = gameWrapper->GetMMRWrapper().GetPlayerMMR(uniqueId, RankedDoubles);
	newRanks.rankedStandard = gameWrapper->GetMMRWrapper().GetPlayerMMR(uniqueId, RankedStandard);
	return newRanks;
}

void PickelTools::onMmrUpdate(UniqueIDWrapper id) {
	if (id != uniqueId) {
		LOG("Received MMR update for unrecognized player: {}", id.GetIdString());
	}

	Ranks newRanks = buildNewRanks();
	if (ranks.rankedDuel != newRanks.rankedDuel ||
		ranks.rankedDoubles != newRanks.rankedDoubles ||
		ranks.rankedStandard != newRanks.rankedStandard) {
		LOG("Rank changed:\nOld ranks: {}\nNew ranks: {}", ranksToString(ranks), ranksToString(newRanks));
		ranks = newRanks;
	}

	if (awaitingFinalMmrUpdate) {
		LOG("Got final MMR update for session");
		awaitingFinalMmrUpdate = false;

		Ranks diff;
		diff.rankedDuel = newRanks.rankedDuel - startSessionRanks.rankedDuel;
		diff.rankedDoubles = newRanks.rankedDoubles - startSessionRanks.rankedDoubles;
		diff.rankedStandard = newRanks.rankedStandard - startSessionRanks.rankedStandard;
		startSessionRanks = {};

		std::stringstream s;
		s << "Completed " << gamesPlayed << " games";
		if (!isNearlyEqual(diff.rankedDuel, 0.f)) {
			s << std::format("\n1v1 {:+.1f}", diff.rankedDuel);
		}
		if (!isNearlyEqual(diff.rankedDoubles, 0.f)) {
			s << std::format("\n2v2 {:+.1f}\n", diff.rankedDoubles);
		}
		if (!isNearlyEqual(diff.rankedStandard, 0.f)) {
			s << std::format("\n3v3 {:+.1f}\n", diff.rankedStandard);
		}
		startTraining();
		gameWrapper->Toast("Session Complete", s.str(), "", 10.0, ToastType_OK);
	}
}

void PickelTools::hookMatchEnded() {
	gameWrapper->HookEventWithCaller<ServerWrapper>(matchEndedEvent, std::bind(&PickelTools::onMatchEnd, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	gameWrapper->HookEventWithCallerPost<ServerWrapper>(penaltyChangedEvent, std::bind(&PickelTools::onPenaltyChanged, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	hooked = true;
}

void PickelTools::unhookMatchEnded() {
	gameWrapper->UnhookEventPost(penaltyChangedEvent);
	gameWrapper->UnhookEvent(matchEndedEvent);
	hooked = false;
}