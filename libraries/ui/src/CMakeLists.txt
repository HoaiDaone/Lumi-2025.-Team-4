include_directories($D:/ArduinoProject/Lumi-2025.-Team-4/libraries/src/ui)

SET(SOURCES 
    ui_comp_bubblechat.c
    ui_comp_conversation.c
    ui_comp_idconversation.c
    ui_comp.c
    ui_MainScreen.c
    ui_MessScreen.c
    ui_HelpScreen.c
    ui_ConversationScreen.c
    ui.c
    ui_comp_hook.c
    ui_helpers.c
    ui_events.c
    ui_img_chat_24dp_000000_fill0_wght300_grad0_opsz24_png.c
    ui_img_help_24dp_000000_fill0_wght300_grad0_opsz24_png.c)

add_library(ui ${SOURCES})
