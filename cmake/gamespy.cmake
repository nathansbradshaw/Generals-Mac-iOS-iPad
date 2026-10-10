set(GS_OPENSSL FALSE)
set(GAMESPY_SERVER_NAME "server.cnc-online.net")

FetchContent_Declare(
    gamespy
    GIT_REPOSITORY https://github.com/TheAssemblyArmada/GamespySDK.git
    GIT_TAG        07e3d15c500415abc281efb74322ab6d9c857eb8
)

FetchContent_MakeAvailable(gamespy)

if(ANDROID)
    # GeneralsX @build BenderAI 13/07/2026 Bionic intentionally omits
    # pthread_cancel. Use the GameSpy SDK's supported synchronous/no-thread
    # mode for the legacy service code; GeneralsOnline uses its own transport.
    target_compile_definitions(gsinterface INTERFACE GSI_NO_THREADS)

    # GeneralsX @build Codex 13/07/2026 Android loads the game as libmain.so,
    # so every GameSpy object folded into its static archive must be PIC.
    target_compile_options(gsinterface INTERFACE -fPIC)
endif()
