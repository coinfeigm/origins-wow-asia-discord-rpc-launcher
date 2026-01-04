#include "DiscordIntegration.h"
#include "discord_rpc.h"
#include <ctime>
#include <cstring>

static bool discord_initialized = false;
static int64_t discord_start_time = 0;
static DiscordEventHandlers discordHandlers;

void InitDiscord()
{
    if (discord_initialized)
        return;

    memset(&discordHandlers, 0, sizeof(discordHandlers));
    Discord_Initialize("YOUR_APP_ID", &discordHandlers, 1, nullptr);

    discord_start_time = time(nullptr);

    DiscordRichPresence presence{};
    presence.state = "Playing Origins WoW Asia";
    presence.startTimestamp = discord_start_time;
    presence.largeImageKey = "origins-wow-asia-logo-no-watermark";
    presence.largeImageText = "Origins WoW Asia";

    Discord_UpdatePresence(&presence);

    discord_initialized = true;
}

void UpdateDiscordPresence()
{
    if (!discord_initialized)
        return;

    DiscordRichPresence presence{};
    presence.state = "Playing Origins WoW Asia";
    presence.startTimestamp = discord_start_time; // SAME TIME
    presence.largeImageKey = "origins-wow-asia-logo-no-watermark";
    presence.largeImageText = "Origins WoW Asia";

    Discord_UpdatePresence(&presence);
}

void ShutdownDiscord()
{
    if (!discord_initialized)
        return;

    Discord_ClearPresence();
    Discord_Shutdown();

    discord_initialized = false;
}
