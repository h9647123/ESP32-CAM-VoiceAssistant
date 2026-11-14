#pragma once
// Display-only markers, never sent to models or used to alter saved source.
static constexpr char RICH_ITALIC_TOGGLE=0x10,RICH_STRIKE_TOGGLE=0x11,
    RICH_SUBHEADING_TOGGLE=0x12,RICH_QUOTE_BAR=0x13;
static bool camRichDecoration(char ch){return ch>=0x10 && ch<=0x13;}
