#pragma once

#include <map>
#include <memory>

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
	static constexpr const char* trainingMapCvarName = "instant_training_map";

	struct Ranks {
		float rankedDuel = 0.f;
		float rankedDoubles = 0.f;
		float rankedStandard = 0.f;
	};

	static std::string ranksToString(const Ranks& ranks);
	static const char* modeToString(Mode mode);

	void pluginEnabledChanged();
	void queue();
	void startTraining();
	void hookMatchEnded();
	void unhookMatchEnded();
	void onMatchEnd(ServerWrapper server, void* params, std::string eventName);
	void onPenaltyChanged(ServerWrapper server, void* params, std::string eventName);
	void onMmrUpdate(UniqueIDWrapper id);
	void startSession();
	void endSession();
	Ranks buildNewRanks();

	Ranks startSessionRanks{};
	Ranks ranks{};
	
	std::string lastMatchGuid;
	UniqueIDWrapper	uniqueId;
	bool hooked = false;
	int gamesRemaining = 0;
	int gamesPlayed = 0;
	Mode gameMode;
	bool awaitingFinalMmrUpdate = false;
	
	std::unique_ptr<MMRNotifierToken> mmrNotifierToken;
};