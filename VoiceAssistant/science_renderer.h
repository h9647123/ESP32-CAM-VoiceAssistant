#pragma once
#include "science_script11.h"
// Included after utf8DecodeAt(), with the project's TFT and style types in scope.
static int scienceTextWidth(const String& text) {
  int width = 0;
  String run;
  for (size_t at = 0; at < text.length();) {
    const size_t len = utf8CharLen(text, at);
    const ScienceGlyph* glyph = scienceGlyph(utf8DecodeAt(text, at, len));
    if (glyph) { width += tft.textWidth(run) + glyph->advance; run = ""; }
    else run += text.substring(at, at + len);
    at += len;
  }
  return width + tft.textWidth(run);
}

static int scienceScriptWidth(const String& text, bool compact=false) {
  int width = 0;
  for (size_t at = 0; at < text.length();) {
    const size_t len = utf8CharLen(text, at);
    const auto* native=compact?nullptr:scienceGlyphLookup(kScienceScript11Glyphs,utf8DecodeAt(text,at,len));
    if(native){width+=native->advance;at+=len;continue;}
    const ScienceSmallGlyph* glyph = scienceSmallGlyph(utf8DecodeAt(text, at, len));
    width += glyph ? glyph->advance :
        tft.textWidth(text.substring(at, at + len), &fonts::efontCN_10);
    at += len;
  }
  return width;
}

static void drawScienceText(LGFX_Sprite* sprite, int x, int y, const String& text,
                            uint16_t color, bool bold, float scale=1.0f,bool italic=false) {
  lgfx::LGFXBase& target = sprite ? static_cast<lgfx::LGFXBase&>(*sprite)
                                : static_cast<lgfx::LGFXBase&>(tft);
  int cursor = x, previousAdvance = 16;
  String run;
  target.setTextSize(scale);
  auto flush = [&]() {
    if (!run.length()) return;
    target.setTextSize(1);const int advance=target.textWidth(run);target.setTextSize(scale);
    target.setTextColor(color); target.setCursor(cursor, y); target.print(run);
    if (bold) { target.setCursor(cursor + 1, y); target.print(run); }
    cursor += int(advance*scale); run = "";
  };
  for (size_t at = 0; at < text.length();) {
    const size_t len = utf8CharLen(text, at);
    const uint32_t cp=utf8DecodeAt(text, at, len);
    const ScienceGlyph* glyph = cp<128 && !italic ? nullptr : scienceGlyph(cp);
    if (glyph) {
      flush();
      const int gx = glyph->advance ? cursor : cursor - previousAdvance + (previousAdvance - 16) / 2;
      for (int row = 0; row < 16; ++row) {
        const uint16_t bits = glyph->rows[row];
        for (int col = 0; col < 16; ++col) if (bits & (0x8000 >> col)) {
          const int shift=italic?(15-row)/6:0;
          target.fillRect(gx+int((col+shift)*scale),y+int(row*scale),max(1,int((col+1)*scale)-int(col*scale)),max(1,int((row+1)*scale)-int(row*scale)),color);
          if (bold && col < 15) target.drawPixel(gx+int((col+shift)*scale)+1,y+int(row*scale),color);
        }
      }
      if (glyph->advance) previousAdvance = glyph->advance;
      cursor += int(glyph->advance*scale);
    } else {
      const String token = text.substring(at, at + len);
      run += token; previousAdvance = tft.textWidth(token);
    }
    at += len;
  }
  flush();
  target.setTextSize(1);
}

static void drawScienceScript(LGFX_Sprite* sprite, int x, int y, const String& text,
                              uint16_t color, bool compact=false) {
  lgfx::LGFXBase& target = sprite ? static_cast<lgfx::LGFXBase&>(*sprite)
                                : static_cast<lgfx::LGFXBase&>(tft);
  for (size_t at = 0; at < text.length();) {
    const size_t len = utf8CharLen(text, at);
    const auto* native=compact?nullptr:scienceGlyphLookup(kScienceScript11Glyphs,utf8DecodeAt(text,at,len));
    if(native) {
      for(int row=0;row<11;++row)for(int col=0;col<native->advance;++col)
        if(native->rows[row] & (0x8000>>col))target.drawPixel(x+col,y+row,color);
      x+=native->advance;at+=len;continue;
    }
    const ScienceSmallGlyph* glyph = scienceSmallGlyph(utf8DecodeAt(text, at, len));
    if (glyph) {
      for (int row = 0; row < 10; ++row) {
        const uint16_t bits = glyph->rows[row];
        for (int col = 0; col < glyph->advance; ++col)
          if (bits & (0x8000 >> col)) {
            target.drawPixel(x+col,y+row,color);
          }
      }
      x += glyph->advance;
    } else {
      const String token = text.substring(at, at + len);
      target.setFont(&fonts::efontCN_10); target.setTextColor(color);
      target.setCursor(x, y); target.print(token);
      x += target.textWidth(token); target.setFont(&fonts::efontCN_16);
    }
    at += len;
  }
}
