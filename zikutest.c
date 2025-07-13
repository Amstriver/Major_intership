#include <stdio.h>
#include "font.h"

int main() {
    Init_Font();

    Clean_Area(0, 0, 800, 480, 0xffffff);

    Display_characterX(0, 0, "XiaoShuai.", 0xff0000, 3);

    return 0;
}   