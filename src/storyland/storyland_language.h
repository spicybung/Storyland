#pragma once
#include <cstdint>
#include <cwchar>
#include "src/storyland/storyland_russian_language.h"

enum class StorylandUiLanguage : uint32_t { English = 0, FrenchQuebec = 1, Italian = 2, Russian = 3 };
inline StorylandUiLanguage gStorylandUiLanguage = StorylandUiLanguage::English;

inline const wchar_t* storylandGameTextFilename(StorylandUiLanguage language) {
    switch (language) {
    case StorylandUiLanguage::FrenchQuebec: return L"FRENCH.GXT";
    case StorylandUiLanguage::Italian: return L"ITALIAN.GXT";
    case StorylandUiLanguage::Russian: return L"RUSSIAN.GXT";
    case StorylandUiLanguage::English: return L"ENGLISH.GXT";
    }
    return L"ENGLISH.GXT";
}

struct StorylandLanguageEntry {
    const wchar_t* english;
    const wchar_t* frenchQuebec;
    const wchar_t* italian;
};

inline const wchar_t* storylandUiText(const wchar_t* english) {
    if (!english || gStorylandUiLanguage == StorylandUiLanguage::English) return english;
    if (gStorylandUiLanguage == StorylandUiLanguage::Russian) return storylandRussianUiText(english);
    static const StorylandLanguageEntry entries[] = {
        {L"File", L"Fichier", L"File"},
        {L"View", L"Affichage", L"Visualizza"},
        {L"Help", L"Aide", L"Aiuto"},
        {L"Option", L"Options", L"Opzioni"},
        {L"Options", L"Options", L"Opzioni"},
        {L"Panels", L"Panneaux", L"Pannelli"},
        {L"Play Options", L"Options de jeu", L"Opzioni di gioco"},
        {L"Language", L"Langue", L"Lingua"},
        {L"English", L"English", L"English"},
        {L"Fran\u00e7ais (Qu\u00e9bec)", L"Fran\u00e7ais (Qu\u00e9bec)", L"Francese (Qu\u00e9bec)"},
        {L"Italiano", L"Italiano", L"Italiano"},
        {L"New", L"Nouveau", L"Nuovo"},
        {L"Open...", L"Ouvrir...", L"Apri..."},
        {L"Open recent", L"Fichiers r\u00e9cents", L"File recenti"},
        {L"Clear list", L"Effacer la liste", L"Svuota elenco"},
        {L"(Empty)", L"(Vide)", L"(Vuoto)"},
        {L"Reload", L"Recharger", L"Ricarica"},
        {L"Exit", L"Quitter", L"Esci"},
        {L"Save", L"Enregistrer", L"Salva"},
        {L"Save As...", L"Enregistrer sous...", L"Salva con nome..."},
        {L"Export...", L"Exporter...", L"Esporta..."},
        {L"Export As...", L"Exporter sous...", L"Esporta con nome..."},
        {L"Export Log...", L"Exporter le journal...", L"Esporta registro..."},
        {L"Export selected resource...", L"Exporter la ressource s\u00e9lectionn\u00e9e...", L"Esporta risorsa selezionata..."},
        {L"Export all extracted resources...", L"Exporter toutes les ressources extraites...", L"Esporta tutte le risorse estratte..."},
        {L"Open selected resource...", L"Ouvrir la ressource s\u00e9lectionn\u00e9e...", L"Apri risorsa selezionata..."},
        {L"Special Thanks...", L"Remerciements sp\u00e9ciaux...", L"Ringraziamenti speciali..."},
        {L"About Storyland...", L"\u00c0 propos de Storyland...", L"Informazioni su Storyland..."},
        {L"Wiki", L"Wiki", L"Wiki"},
        {L"Viewport options", L"Options de la vue 3D", L"Opzioni vista 3D"},
        {L"Stories shader", L"Ombrage Stories", L"Shader Stories"},
        {L"Textured", L"Textur\u00e9", L"Con texture"},
        {L"Solid", L"Solide", L"Solido"},
        {L"Wireframe", L"Fil de fer", L"Reticolo"},
        {L"Grid", L"Grille", L"Griglia"},
        {L"Bones", L"Os", L"Ossa"},
        {L"Bounds", L"Limites", L"Limiti"},
        {L"2DFX lights", L"Lumi\u00e8res 2DFX", L"Luci 2DFX"},
        {L"View cube", L"Cube de vue", L"Cubo di vista"},
        {L"Play Test", L"Essai en jeu", L"Prova di gioco"},
        {L"Stop Play Test", L"Arr\u00eater l'essai en jeu", L"Ferma prova di gioco"},
        {L"Sky", L"Ciel", L"Cielo"},
        {L"Enabled", L"Activ\u00e9", L"Attivo"},
        {L"Midnight", L"Minuit", L"Mezzanotte"},
        {L"Dawn", L"Aube", L"Alba"},
        {L"Noon", L"Midi", L"Mezzogiorno"},
        {L"Sunset", L"Coucher du soleil", L"Tramonto"},
        {L"Weather", L"M\u00e9t\u00e9o", L"Meteo"},
        {L"Sunny", L"Ensoleill\u00e9", L"Soleggiato"},
        {L"Clear", L"D\u00e9gag\u00e9", L"Sereno"},
        {L"Cloudy", L"Nuageux", L"Nuvoloso"},
        {L"Rainy", L"Pluvieux", L"Piovoso"},
        {L"Foggy", L"Brumeux", L"Nebbioso"},
        {L"Windy", L"Venteux", L"Ventoso"},
        {L"Thunderstorm", L"Orage", L"Temporale"},
        {L"Background", L"Arri\u00e8re-plan", L"Sfondo"},
        {L"Light", L"Clair", L"Chiaro"},
        {L"Dark", L"Sombre", L"Scuro"},
        {L"Texture", L"Texture", L"Texture"},
        {L"Flip vertically", L"Retourner verticalement", L"Capovolgi verticalmente"},
        {L"Flip V coordinate", L"Inverser les coordonn\u00e9es V", L"Inverti coordinate V"},
        {L"Larger texture size", L"Texture plus grande", L"Texture pi\u00f9 grande"},
        {L"Smaller texture size", L"Texture plus petite", L"Texture pi\u00f9 piccola"},
        {L"Pixel order", L"Ordre des pixels", L"Ordine pixel"},
        {L"Automatic", L"Automatique", L"Automatico"},
        {L"As stored", L"Tel qu\u2019enregistr\u00e9", L"Come memorizzato"},
        {L"PS2 order", L"Ordre PS2", L"Ordine PS2"},
        {L"PSP order", L"Ordre PSP", L"Ordine PSP"},
        {L"Dear ImGui workspace", L"Espace de travail Dear ImGui", L"Area di lavoro Dear ImGui"},
        {L"Scripting...", L"Scripts...", L"Script..."},
        {L"Simple Model", L"Mod\u00e8le simple", L"Modello semplice"},
        {L"Ped Model", L"Mod\u00e8le de pi\u00e9ton", L"Modello pedone"},
        {L"Cutscene Model", L"Mod\u00e8le de cin\u00e9matique", L"Modello filmato"},
        {L"Vehicle Model", L"Mod\u00e8le de v\u00e9hicule", L"Modello veicolo"},
        {L"World Model", L"Mod\u00e8le du monde", L"Modello mondo"},
        {L"Mobile", L"Mobile", L"Mobile"},
        {L"Rebuild...", L"Reconstruire...", L"Ricostruisci..."},
        {L"Rebuild As...", L"Reconstruire sous...", L"Ricostruisci come..."},
        {L"Rebuild and Play Test", L"Reconstruire et essayer", L"Ricostruisci e prova"},
        {L"Find text...", L"Rechercher du texte...", L"Cerca testo..."},
        {L"Edit selected string...", L"Modifier le texte s\u00e9lectionn\u00e9...", L"Modifica testo selezionato..."},
        {L"Save GXT", L"Enregistrer GXT", L"Salva GXT"},
        {L"Save GXT As...", L"Enregistrer GXT sous...", L"Salva GXT con nome..."},
        {L"Export Text...", L"Exporter le texte...", L"Esporta testo..."},
        {L"Character encoding", L"Encodage des caract\u00e8res", L"Codifica caratteri"},
        {L"Save data", L"Enregistrer les donn\u00e9es", L"Salva dati"},
        {L"Save data As...", L"Enregistrer les donn\u00e9es sous...", L"Salva dati con nome..."},
        {L"Find data...", L"Rechercher des donn\u00e9es...", L"Cerca dati..."},
        {L"Edit selected row / field...", L"Modifier la ligne ou le champ...", L"Modifica riga o campo..."},
        {L"Add row...", L"Ajouter une ligne...", L"Aggiungi riga..."},
        {L"Remove row", L"Supprimer la ligne", L"Rimuovi riga"},
        {L"Move row up", L"Monter la ligne", L"Sposta riga su"},
        {L"Move row down", L"Descendre la ligne", L"Sposta riga gi\u00f9"},
        {L"Check data", L"V\u00e9rifier les donn\u00e9es", L"Verifica dati"},
        {L"Check file references", L"V\u00e9rifier les r\u00e9f\u00e9rences", L"Verifica riferimenti"},
        {L"Enable / disable command", L"Activer ou d\u00e9sactiver la commande", L"Attiva o disattiva comando"},
        {L"Open referenced file", L"Ouvrir le fichier r\u00e9f\u00e9renc\u00e9", L"Apri file di riferimento"},
        {L"Resources", L"Ressources", L"Risorse"},
        {L"Details", L"D\u00e9tails", L"Dettagli"},
        {L"Actions", L"Actions", L"Azioni"},
        {L"Analyze", L"Analyser", L"Analizza"},
        {L"Scripting", L"Scripts", L"Script"},
        {L"Play Test Debug Menu", L"Menu de d\u00e9bogage du jeu", L"Menu debug gioco"},
        {L"Mission objective", L"Objectif de mission", L"Obiettivo missione"},
        {L"Mission script", L"Script de mission", L"Script missione"},
        {L"SCM execution result", L"R\u00e9sultat d'ex\u00e9cution SCM", L"Risultato esecuzione SCM"},
        {L"Model collision", L"Collision du mod\u00e8le", L"Collisione modello"},
        {L"Storyland Resources", L"Ressources Storyland", L"Risorse Storyland"},
        {L"Restore", L"Restaurer", L"Ripristina"},
        {L"Maximize", L"Agrandir", L"Ingrandisci"},
        {L"Random Street Weapons", L"Armes al\u00e9atoires dans la rue", L"Armi casuali per strada"},
        {L"Refocus behind player", L"Recentrer derri\u00e8re le joueur", L"Ricentra dietro il giocatore"},
        {L"Native paths", L"Trajets natifs", L"Percorsi nativi"},
        {L"Collision surfaces (native COL2)", L"Surfaces de collision (COL2 natif)", L"Superfici di collisione (COL2 nativo)"},
        {L"LCS/VCS player camera", L"Cam\u00e9ra du joueur LCS/VCS", L"Telecamera giocatore LCS/VCS"},
        {L"   Play...", L"   Jouer...", L"   Gioca..."},
        {L"   Play", L"   Jouer", L"   Gioca"},
        {L"(I'm not affiliated with some of these peeps)",
         L"(Je ne suis pas affili\u00e9 \u00e0 certaines de ces personnes)",
         L"(Non sono affiliato ad alcune di queste persone)"},
        {L"SPECIAL THANKS", L"REMERCIEMENTS SP\u00c9CIAUX", L"RINGRAZIAMENTI SPECIALI"},
    };
    for (const auto& entry : entries) {
        if (std::wcscmp(english, entry.english) == 0)
            return gStorylandUiLanguage == StorylandUiLanguage::FrenchQuebec ? entry.frenchQuebec :
                gStorylandUiLanguage == StorylandUiLanguage::Italian ? entry.italian : english;
    }
    return english;
}
