#pragma once

struct StorylandAndroidCredit { const char* name; const char* contribution; };

inline constexpr StorylandAndroidCredit storylandAndroidCredits[] = {
    {"Zeke Luna", "For keeping some company when things were hard"},
    {"Dad", "For his love and patience"},
    {"Daniil Sayanov", "For his awesome knowledge, contributions to my learning, and friendship"},
    {"by-tndy", "For support and contributions"},
    {"Hero (aap)", "For his wealth of knowledge and amazing reverse engineering skills"},
    {"Guard3", "For his knowledge of DTZ formats"},
    {"DK22Pac", "For his knowledge of formats and pioneering Chinatown Wars efforts"},
    {"Kam", "One of the OGs, helping shape the GTA modding scene as we know it today"},
    {"Allen", "for his knowledge in Manhunt 2 formats."},
    {"Ruby", "With thanks and appreciation"},
    {"Rockstar Games", "(but not Take-Two, ewww)"},
    {"Rockstar Leeds", ""},
    {"Jessica Natalia", "for her work on PSP recompilation/formats"},
    {"AK73", "for the original LCS/VCS IO template, an early Stories MDL importer, and LCS conversion tooling"},
    {"Edison Carter", "Created the original LCS/VCS CheatDevice, enabling memory editing, vehicle spawning and teleportation (2005-2007)"},
    {"Dustin Holden (Dustcrazy)", "Developed YAIE (Yet Another IMG Editor), an early LCS/VCS PSP and PS2 IMG editor (2007)"},
    {"Aru", "Developed GTAStories Texture Viewer for examining LCS and VCS textures (2000s)"},
    {"Dageron / Bbobik", "Led LCS PC Edition development and created Stories texture tools (2007-2013)"},
    {"gamerzworld", "Documented early VCS character-model replacements, including Victor, Lance Vance and Phil Collins (2007)"},
    {"matt11504", "Published guidance for replacing VCS character models and textures in PSP archives (2007)"},
    {"Viger74", "Worked on animation conversion, mission-script debugging and audio code for LCS PC Edition (2009-2010s)"},
    {"HackMan128 / ModMaker", "Worked on LCS PC Edition models, HUD, map objects and related conversions (2009-2010s)"},
    {"Ash_735", "Contributed texture and sound conversion expertise to LCS PC Edition (2009-2010s)"},
    {"XEPOMAHT007", "modeling, texturing"},
    {"MAJESTIC_R3", "Author of Stories Map Converter; his work helped with LVZ + IMG knowledge"},
};

inline constexpr const char* storylandAndroidQuotes[] = {
    "No, you're the asshole who got kicked out of the boy scouts. - Vic",
    "I am Lance Vance, baby - you can trust me. Lance T Vance - T for 'Trust'... - Lance",
    "Try to keep up, man, I'm a baaad driver. - Lance",
    "What the fuck are you talking about? - Vic",
    "Let me squeeze a fart out of ya. - Phil",
    "Yeah, fools seldom differ, dipshit. - Forbes",
    "Marty can do it, and he was nearly inbred. You'll pick it up! - Louise",
    "I play around all the time - but don't tell my wife. - Gonzalez",
    "St. Victor of Vance, the holier than thou killer! - Martinez",
    "Play time is over, bitches! - Lance",
    "Ah shit, here we go again. - CJ",
    "Grove Street - Home. At least it was before I fucked everything up. - CJ",
    "I am the real deal, fool. Oh yeah. A genius. - Ryder",
    "Keep up, motherfucker! - Ryder",
    "For Grove Street, baby! - Sweet",
    "GROVE IS KING!! - Sweet",
    "Vercetti, remember the name! - Tommy",
    "I run this town now - me! - Tommy",
    "He killed my brother. What do you expect me to do, mow his lawns? - Lance",
    "Son, I could shoot a fly off your head at 80 feet. - Phil",
    "We made it! We're rich! RIIIIICH! - Phil",
    "I'm too crafty for that, sunshine! - Kent Paul",
    "Bloody hell... you nutter! - Kent Paul",
};

inline constexpr char32_t storylandAndroidStudioLogo[] = UR"LOGO( ▄▄▄▄▄▄▄       ▄▄▄▄  ▄▄▄     ▄▄▄▄▄    ▄▄▄   ▄▄▄    ▄▄▄▄▄▄▄        ▄▄▄▄▄▄▄  ▄▄▄▄▄▄▄▄▄  ▄▄▄   ▄▄▄ ▄▄▄▄▄▄▄     ▄▄▄     ▄▄▄       ▄▄▄▄▄▄▄
▐███▓▓▓▓▀▄  ▄██████▌▐███▌ ▄███████▌  ▐███▄ ▐███▌ ▄████████▌     ▄████████▌▐█████████▌▐███▌ ▐███▌▐████████▄ ▐███▌ ▄███████▄  ▄████████▌
▐░░░▌ ▐░░▐ ▐░░░█▀   ▐░░░▌▐░░░█▀      ▐░░░░▌▐░░░▌▐░░░▌ ▐░░░▌    ▐░░░▌ ▐░░░▌   ▐░░░▌   ▐░░░▌ ▐░░░▌▐░░░▌ ▐░░░▌▐░░░▌▐░░░▌ ▐░░░▌▐░░░▌ ▐░░░▌
▐▒▒▒▌  ▒▒▐ ▐▒▒▒▌    ▐▒▒▒▌▐▒▒▒▌       ▐▒▒▒▌▌▐▒▒▒▌▐▒▒▒▌ ▐▒▒▒▌    ▐▒▒▒▌ ▐▒▒▒▌   ▐▒▒▒▌   ▐▒▒▒▌ ▐▒▒▒▌▐▒▒▒▌ ▐▒▒▒▌▐▒▒▒▌▐▒▒▒▌ ▐▒▒▒▌▐▒▒▒▌ ▐▒▒▒▌
▐▓▓▓▌ ▐▓▓▌ ▐▓▓▓▓▓▓  ▐▓▓▓▌▐▓▓▓▌██████▌▐▓▓▓▌▐▐▓▓▓▌▐▓▓▓▌  ▀▀▀     ▐▓▓▓▌  ▀▀▀    ▐▓▓▓▌   ▐▓▓▓▌ ▐▓▓▓▌▐▓▓▓▌ ▐▓▓▓▌▐▓▓▓▌▐▓▓▓▌ ▐▓▓▓▌▐▓▓▓▌  ▀▀▀
▐███▌ ███  ▐████▀▀  ▐███▌▐███▌ ▐███▌ ▐███▌▐▌███▌ ▀███████▄      ▀███████▄    ▐███▌   ▐███▌ ▐███▌▐███▌ ▐███▌▐███▌▐███▌ ▐███▌ ▀███████▄
▐░░░▌▐░░▌  ▐░░░▌    ▐░░░▌▐░░░▌ ▐░░░▌ ▐░░░▌ ▌░░░▌ ▄▄▄  ▐░░░▌     ▄▄▄  ▐░░░▌   ▐░░░▌   ▐░░░▌ ▐░░░▌▐░░░▌ ▐░░░▌▐░░░▌▐░░░▌ ▐░░░▌ ▄▄▄  ▐░░░▌
▐▒▒▒▌▓▒█   ▐▒▒▒▌    ▐▒▒▒▌▐▒▒▒▌ ▐▒▒▒▌ ▐▒▒▒▌ ▐▒▒▒▌▐▒▒▒▌ ▐▒▒▒▌    ▐▒▒▒▌ ▐▒▒▒▌   ▐▒▒▒▌   ▐▒▒▒▌ ▐▒▒▒▌▐▒▒▒▌ ▐▒▒▒▌▐▒▒▒▌▐▒▒▒▌ ▐▒▒▒▌▐▒▒▒▌ ▐▒▒▒▌
▐▓▓▓▌▐▓▓█▄ ▐▓▓▓▓▄▄▄ ▐▓▓▓▌▐▓▓▓▓▄▓▓▓▓▓▌▐▓▓▓▌ ▐▓▓▓▌▐▓▓▓▌ ▐▓▓▓▌    ▐▓▓▓▌ ▐▓▓▓▌   ▐▓▓▓▌    ▌▓▓▓▄▓▓▓▐ ▐▓▓▓▌ ▐▓▓▓▌▐▓▓▓▌▐▓▓▓▌ ▐▓▓▓▌▐▓▓▓▌ ▐▓▓▓▌
▐███▌ ▀███▌ ▀▄█████▌▐███▌ ▀▄████████▌▐███▌ ▐███▌▐████████▀     ▐████████▀    ▐███▌    ▀▄█████▄▀ ▐████████▀ ▐███▌ ▀███████▀ ▐████████▀)LOGO";
