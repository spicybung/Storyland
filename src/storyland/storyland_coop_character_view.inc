static void storylandRestoreCoopPlayerModel() {
    if (gCoopOriginalPlayer) {
        gPlayTestPlayerModel = std::move(gCoopOriginalPlayer->model);
        gPlayTestPlayerImages = std::move(gCoopOriginalPlayer->images);
        gPlayTestPlayerTextureNames = std::move(gCoopOriginalPlayer->textureNames);
        gPlayTestPlayerName = std::move(gCoopOriginalPlayer->name);
        gCoopOriginalPlayer.reset();
        gPlayTestPlayerBlend = {};
        gPlayTestPlayerAnimation.configureCombatDefinitions(gPlayTestRuntime.weapons, &gPlayTestPlayerModel);
        ++gPlayTestGraphicsRevision;
    }
    gCoopLocalCharacterName.clear();
    gCoopLocalMaterialColors.clear();
    gCoopRemoteCharacterName.clear();
    gCoopRemoteCharacter.reset();
}

static bool storylandUpdateCoopCharacterModels() {
    if (!gMultiplayer.connected()) {
        storylandRestoreCoopPlayerModel();
        return true;
    }
    const bool vcs = gDtzArchive.gameVariant() == StorylandDtzGameVariant::Vcs;
    const std::string guestName = gMultiplayer.guestModelName(vcs);
    if (!gMultiplayer.isHost() && gCoopLocalCharacterName != guestName) {
        StorylandPlayTestAsset replacement;
        std::string error;
        if (!storylandLoadCoopCharacter(gDtzArchive, guestName, replacement, error)) {
            gPlayTestStatus = error;
            return false;
        }
        if (!gCoopOriginalPlayer) {
            gCoopOriginalPlayer = std::make_unique<StorylandPlayTestAsset>();
            gCoopOriginalPlayer->model = std::move(gPlayTestPlayerModel);
            gCoopOriginalPlayer->images = std::move(gPlayTestPlayerImages);
            gCoopOriginalPlayer->textureNames = std::move(gPlayTestPlayerTextureNames);
            gCoopOriginalPlayer->name = std::move(gPlayTestPlayerName);
        }
        gPlayTestPlayerModel = std::move(replacement.model);
        gPlayTestPlayerImages = std::move(replacement.images);
        gPlayTestPlayerTextureNames = std::move(replacement.textureNames);
        gPlayTestPlayerName = std::move(replacement.name);
        gCoopLocalMaterialColors = std::move(replacement.materialColors);
        gCoopLocalCharacterName = guestName;
        gPlayTestPlayerBlend = {};
        gPlayTestPlayerAnimation.configureCombatDefinitions(gPlayTestRuntime.weapons, &gPlayTestPlayerModel);
        ++gPlayTestGraphicsRevision;
    }
    std::string remoteName = guestName;
    if (!gMultiplayer.isHost()) {
        size_t entry = 0;
        if (gMultiplayer.remote.model >= 0 &&
            gDtzArchive.findModelDirEntryByModelId(uint32_t(gMultiplayer.remote.model), entry, remoteName))
            remoteName = std::filesystem::path(remoteName).stem().u8string();
        else remoteName = std::filesystem::path(gCoopOriginalPlayer ?
            gCoopOriginalPlayer->name : gPlayTestPlayerName).stem().u8string();
    }
    if (gCoopRemoteCharacterName != remoteName) {
        auto replacement = std::make_unique<StorylandPlayTestAsset>();
        std::string error;
        if (!storylandLoadCoopCharacter(gDtzArchive, remoteName, *replacement, error)) {
            gPlayTestStatus = error;
            return false;
        }
        gCoopRemoteCharacter = std::move(replacement);
        gCoopRemoteCharacterName = remoteName;
        gPlayTestRemoteBlend = {};
        ++gPlayTestGraphicsRevision;
    }
    return true;
}
