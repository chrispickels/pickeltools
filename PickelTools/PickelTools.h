#pragma once

#include <map>
#include <memory>
#include <optional>

#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"

#include "version.h"
constexpr auto plugin_version = stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(VERSION_PATCH) "." stringify(VERSION_BUILD);

class PickelTools final : public BakkesMod::Plugin::BakkesModPlugin, public BakkesMod::Plugin::PluginSettingsWindow {
public:
	void onLoad() override;
	void onUnload() override;
	
	void RenderSettings() override;
	std::string GetPluginName() override;
	void SetImGuiContext(uintptr_t ctx) override;

private:
	enum Mode
	{
		CasualDuel = 1,
		CasualDoubles = 2,
		CasualStandard = 3,
		CasualChaos = 4,
		Private = 6,
		RankedDuel = 10,
		RankedDoubles = 11,
		RankedSoloStandard = 12,
		RankedStandard = 13,
		MutatorMashup = 14,
		Tournament = 22,
		RankedHoops = 27,
		RankedRumble = 28,
		RankedDropshot = 29,
		RankedSnowday = 30,
		GodBall = 38,
		GodBallDoubles = 43
	};

	static constexpr const char* matchEndedEvent = "Function TAGame.GameEvent_Soccar_TA.EventMatchEnded";
	static constexpr const char* penaltyChangedEvent = "Function TAGame.GameEvent_TA.EventPenaltyChanged";
	static constexpr const char* enabledCvarName = "pickel_tools_enabled";
	static constexpr const char* trainingCvarName = "instant_training_enabled";
	static constexpr const char* queueCvarName = "instant_queue_enabled";
	static constexpr const char* exitCvarName = "instant_exit_enabled";
	static constexpr const char* trainingMapCvarName = "instant_training_map";
	static constexpr const char* tDelayCvarName = "instant_training_delay";
	static constexpr const char* qDelayCvarName = "instant_queue_delay";
	static constexpr const char* eDelayCvarName = "instant_exit_delay";
	static constexpr const char* disableCasualQCvarName = "instant_queue_bypass_casual";
	static constexpr const char* disableCasualTCvarName = "instant_training_bypass_casual";
	static constexpr const char* disableCasualECvarName = "instant_exit_bypass_casual";
	static constexpr const char* disablePrivateCvarName = "pickel_tools_private_disable";

	struct Ranks {
		float rankedDuel = 0.f;
		float rankedDoubles = 0.f;
		float rankedStandard = 0.f;
	};

	static std::string ranksToString(const Ranks& ranks);
	static const char* modeToString(Mode mode);

	void pluginEnabledChanged();
	void launchTraining(ServerWrapper caller, void* params, std::string eventName);
	void queue(ServerWrapper caller, void* params, std::string eventName);
	void exitGame(ServerWrapper caller, void* params, std::string eventName);
	void delayedQueue();
	void delayedTraining();
	void delayedExit();
	void hookMatchEnded();
	void unhookMatchEnded();
	void onMatchEnd(ServerWrapper server, void* params, std::string eventName);
	void onPenaltyChanged(ServerWrapper server, void* params, std::string eventName);
	void onMmrUpdate(UniqueIDWrapper id);
	void clearPlaylists(MatchmakingWrapper& mm);
	void startSession();
	void endSession();
	Ranks buildNewRanks();

	std::optional<Ranks> ranks;
	
	std::string lastMatchGuid;
	UniqueIDWrapper	uniqueId;
	bool hooked = false;
	std::map<int, float> playerMmr;
	uintptr_t imguiCtx;
	int gamesRemaining = 0;
	int gamesPlayed = 0;
	Mode gameMode;
	
	std::unique_ptr<MMRNotifierToken> mmrNotifierToken;
};