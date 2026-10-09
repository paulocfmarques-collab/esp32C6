#include <vector>

std::vector<String> linhas;

const int FONT_SIZE = 2;
const int CHAR_W = 12;    // aproximadamente para TextSize(2)
const int CHAR_H = 16;
const int MARGIN = 2;

void printTela(String texto)
{
  int maxCols = gfx->width() / CHAR_W;

  while (texto.length() > maxCols)
  {
    linhas.push_back(texto.substring(0, maxCols));
    texto = texto.substring(maxCols);
  }

  linhas.push_back(texto);

  int maxLinhas = gfx->height() / CHAR_H;

  while (linhas.size() > maxLinhas)
  {
    linhas.erase(linhas.begin());
  }

  gfx->fillScreen(0x0000);
  gfx->setTextColor(0xFFFF);
  gfx->setTextSize(FONT_SIZE);

  int y = 0;

  for (size_t i = 0; i < linhas.size(); i++)
  {
    gfx->setCursor(MARGIN, y);
    gfx->println(linhas[i]);
    y += CHAR_H;
  }
}