
set(GOLF_SRC
  ${PROJECT_DIR}/golf/BallAnimationSystem.cpp
  ${PROJECT_DIR}/golf/BallSystem.cpp
  ${PROJECT_DIR}/golf/BallTrail.cpp
  ${PROJECT_DIR}/golf/BilliardsClientCollision.cpp
  ${PROJECT_DIR}/golf/BilliardsInput.cpp
  ${PROJECT_DIR}/golf/BilliardsSoundDirector.cpp
  ${PROJECT_DIR}/golf/BilliardsState.cpp
  ${PROJECT_DIR}/golf/BilliardsStateUI.cpp
  ${PROJECT_DIR}/golf/BilliardsSystem.cpp
  #${PROJECT_DIR}/golf/BilliardsSystemReact.cpp
  ${PROJECT_DIR}/golf/ButtonHoldSystem.cpp
  ${PROJECT_DIR}/golf/CameraFollowSystem.cpp
  ${PROJECT_DIR}/golf/Career.cpp
  ${PROJECT_DIR}/golf/CareerState.cpp
  ${PROJECT_DIR}/golf/ChunkVisSystem.cpp
  ${PROJECT_DIR}/golf/ClientCollisionSystem.cpp
  ${PROJECT_DIR}/golf/CloudSystem.cpp
  ${PROJECT_DIR}/golf/ClubhouseState.cpp
  ${PROJECT_DIR}/golf/ClubModels.cpp
  ${PROJECT_DIR}/golf/ClubhouseStateUI.cpp
  ${PROJECT_DIR}/golf/Clubs.cpp
  ${PROJECT_DIR}/golf/CoinSystem.cpp
  ${PROJECT_DIR}/golf/CollisionMesh.cpp
  ${PROJECT_DIR}/golf/CPUGolfer.cpp
  ${PROJECT_DIR}/golf/CreditsState.cpp
  ${PROJECT_DIR}/golf/DebugDraw.cpp
  ${PROJECT_DIR}/golf/DrivingRangeDirector.cpp
  ${PROJECT_DIR}/golf/DrivingState.cpp
  ${PROJECT_DIR}/golf/DrivingStateUI.cpp
  ${PROJECT_DIR}/golf/ErrorState.cpp
  ${PROJECT_DIR}/golf/FireworksSystem.cpp
  ${PROJECT_DIR}/golf/FloatingTextSystem.cpp
  ${PROJECT_DIR}/golf/FpsCameraSystem.cpp
  ${PROJECT_DIR}/golf/FreePlayState.cpp
  ${PROJECT_DIR}/golf/GameConsts.cpp
  ${PROJECT_DIR}/golf/GcState.cpp
  ${PROJECT_DIR}/golf/GolfCartSystem.cpp
  ${PROJECT_DIR}/golf/GolfParticleDirector.cpp
  ${PROJECT_DIR}/golf/GolfSoundDirector.cpp
  ${PROJECT_DIR}/golf/GolfState.cpp
  ${PROJECT_DIR}/golf/GolfStateAssets.cpp
  ${PROJECT_DIR}/golf/GolfStateCameras.cpp
  ${PROJECT_DIR}/golf/GolfStateDebug.cpp
  ${PROJECT_DIR}/golf/GolfStateScoring.cpp
  ${PROJECT_DIR}/golf/GolfStateUI.cpp
  ${PROJECT_DIR}/golf/InputParser.cpp  
#  ${PROJECT_DIR}/golf/InterpolationComponent.cpp
#  ${PROJECT_DIR}/golf/InterpolationSystem.cpp
  ${PROJECT_DIR}/golf/KeyboardState.cpp
  ${PROJECT_DIR}/golf/LeaderboardState.cpp
  ${PROJECT_DIR}/golf/LeaderboardTexture.cpp
  ${PROJECT_DIR}/golf/League.cpp
  ${PROJECT_DIR}/golf/LeagueNames.cpp
  ${PROJECT_DIR}/golf/LeagueState.cpp
  ${PROJECT_DIR}/golf/LightAnimationSystem.cpp
  ${PROJECT_DIR}/golf/LightmapProjectionSystem.cpp
  ${PROJECT_DIR}/golf/MenuAvatars.cpp
  ${PROJECT_DIR}/golf/MapOverviewState.cpp
  ${PROJECT_DIR}/golf/MenuCallbacks.cpp
  ${PROJECT_DIR}/golf/MenuCreation.cpp
  ${PROJECT_DIR}/golf/MenuCustomisation.cpp
  ${PROJECT_DIR}/golf/MenuSoundDirector.cpp
  ${PROJECT_DIR}/golf/MenuState.cpp
  ${PROJECT_DIR}/golf/MenuStateCan.cpp
  ${PROJECT_DIR}/golf/MessageOverlayState.cpp
  ${PROJECT_DIR}/golf/MiniBallSystem.cpp
  ${PROJECT_DIR}/golf/NameScrollSystem.cpp
  ${PROJECT_DIR}/golf/NewsState.cpp
  ${PROJECT_DIR}/golf/NotificationSystem.cpp
  ${PROJECT_DIR}/golf/OptionsState.cpp
  ${PROJECT_DIR}/golf/PauseState.cpp
  ${PROJECT_DIR}/golf/ParticleDirector.cpp
  ${PROJECT_DIR}/golf/Path.cpp
  ${PROJECT_DIR}/golf/PlayerAvatar.cpp
  ${PROJECT_DIR}/golf/PlayerData.cpp
  ${PROJECT_DIR}/golf/PlayerManagementState.cpp
  ${PROJECT_DIR}/golf/PlaylistState.cpp
  ${PROJECT_DIR}/golf/PocketBallSystem.cpp
  ${PROJECT_DIR}/golf/PracticeState.cpp
  ${PROJECT_DIR}/golf/ProfileState.cpp
  ${PROJECT_DIR}/golf/ProgressIcon.cpp
  ${PROJECT_DIR}/golf/PropFollowSystem.cpp
  #${PROJECT_DIR}/golf/PuttingState.cpp
  #${PROJECT_DIR}/golf/PuttingStateUI.cpp
  ${PROJECT_DIR}/golf/RayResultCallback.cpp
  ${PROJECT_DIR}/golf/RopeSystem.cpp
  ${PROJECT_DIR}/golf/SharedStateData.cpp
  ${PROJECT_DIR}/golf/SoundEffectsDirector.cpp
  ${PROJECT_DIR}/golf/SpectatorSystem.cpp
  ${PROJECT_DIR}/golf/spooky2.cpp
  ${PROJECT_DIR}/golf/StatsState.cpp
  ${PROJECT_DIR}/golf/StudioCameraSystem.cpp
  ${PROJECT_DIR}/golf/Swingput.cpp
  ${PROJECT_DIR}/golf/TableData.cpp
  ${PROJECT_DIR}/golf/TerrainBuilder.cpp
  ${PROJECT_DIR}/golf/TerrainChunks.cpp
  ${PROJECT_DIR}/golf/TerrainDepthmap.cpp
  ${PROJECT_DIR}/golf/TextChat.cpp
  ${PROJECT_DIR}/golf/TimeOfDay.cpp
  ${PROJECT_DIR}/golf/Tournament.cpp
  ${PROJECT_DIR}/golf/TournamentState.cpp
  ${PROJECT_DIR}/golf/TrophyDisplaySystem.cpp
  ${PROJECT_DIR}/golf/TrophyState.cpp
  ${PROJECT_DIR}/golf/TutorialDirector.cpp
  ${PROJECT_DIR}/golf/TutorialState.cpp
  ${PROJECT_DIR}/golf/UnlockState.cpp
  ${PROJECT_DIR}/golf/VatAnimationSystem.cpp
  ${PROJECT_DIR}/golf/VatFile.cpp
  ${PROJECT_DIR}/golf/VoiceChat.cpp
  ${PROJECT_DIR}/golf/Weather.cpp
  ${PROJECT_DIR}/golf/WeatherAnimationSystem.cpp
  ${PROJECT_DIR}/golf/WeatherDirector.cpp

  #${PROJECT_DIR}/golf/server/GolfDefaultDirector.cpp
  ${PROJECT_DIR}/golf/server/EightballDirector.cpp
  ${PROJECT_DIR}/golf/server/NineballDirector.cpp
  ${PROJECT_DIR}/golf/server/Server.cpp
  ${PROJECT_DIR}/golf/server/ServerBilliardsState.cpp
  ${PROJECT_DIR}/golf/server/ServerGolfRules.cpp
  ${PROJECT_DIR}/golf/server/ServerGolfState.cpp
  ${PROJECT_DIR}/golf/server/ServerLobbyGame.cpp
  ${PROJECT_DIR}/golf/server/ServerLobbyState.cpp
  ${PROJECT_DIR}/golf/server/ServerVoice.cpp
  ${PROJECT_DIR}/golf/server/SnookerDirector.cpp)
