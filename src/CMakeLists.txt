SET(SOURCES ui_Screen1.c
    ui.c
    ui_comp_hook.c
    ui_helpers.c
    fonts/ui_font_Euro10.c
    fonts/ui_font_Euro20.c
    fonts/ui_font_euro80.c)

add_library(ui ${SOURCES})
