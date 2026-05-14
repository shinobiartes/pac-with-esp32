// by shinobiartes

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

// --- PINAGEM ESP32-S3 SUPERMINI (Conforme sua configuração) ---
#define TFT_CS    5   // Chip Select do Display
#define TFT_RST   6   // Reset do Display
#define TFT_DC    7   // Data/Command do Display
#define TFT_SDA   3   // MOSI (Dados SPI)
#define TFT_SCK   2   // Clock (SPI)
#define PIN_UP    10  // Botão Cima
#define PIN_DOWN  11  // Botão Baixo
#define PIN_LEFT  12  // Botão Esquerda
#define PIN_RIGHT 13  // Botão Direita
#define PIN_FIRE  1   // Botão Tiro (Iniciar)
#define PIN_BUZZER 4  // Saída de Áudio

// Instância do Display ST7735
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// Configurações lógicas
#define TAM_BLOCO 16
#define PRETO     0x0000
#define AZUL      0x001F
#define AMARELO   0xFFE0
#define BRANCO    0xFFFF
#define VERMELHO  0xF800
#define ROSA      0xFC18
#define CIANO     0x07FF

// Variáveis globais do jogo
int pacX, pacY, dirX, dirY, proxDirX, proxDirY;
int score, fase = 1, pastilhasRestantes;
int gameState = 0; 
bool imortal = false;
unsigned long tempoImortal = 0;

// Estrutura para os Fantasmas
struct Fantasma {
  float x, y, dx, dy;
  uint16_t cor;
  bool ativo, vivo;
  unsigned long tempoRenascer;
};
Fantasma fantasmas[4];
byte mapa[8][10]; // Matriz do labirinto

// --- FUNÇÕES DE ÁUDIO ---

// Toque de trombeta para início de fase
void somInicioFase() {
  int notas[] = {392, 523, 659, 784}; // Sol, Dó, Mi, Sol
  for (int i = 0; i < 4; i++) {
    tone(PIN_BUZZER, notas[i], 150);
    delay(200);
  }
  noTone(PIN_BUZZER);
}

// Melodia "Despacito" simplificada para o Buzzer
void tocarDespacito() {
  int notas[] = {587, 493, 370, 370, 370, 370, 440, 493, 554, 440, 440, 440, 440, 440, 440, 493, 554, 440, 370};
  int duracao[] = {400, 400, 100, 100, 100, 100, 100, 100, 400, 100, 100, 100, 100, 100, 100, 100, 100, 100, 400};
  for (int i = 0; i < 19; i++) {
    tone(PIN_BUZZER, notas[i], duracao[i]);
    delay(duracao[i] * 1.3);
  }
  noTone(PIN_BUZZER);
}

// --- FUNÇÕES DE DESENHO ---

// Desenha os olhos clássicos para Pac-Man e Fantasmas
void desenharOlhos(int x, int y) {
  tft.fillCircle(x + 5, y + 5, 2, BRANCO);
  tft.fillCircle(x + 11, y + 5, 2, BRANCO);
  tft.drawPixel(x + 5, y + 5, PRETO);
  tft.drawPixel(x + 11, y + 5, PRETO);
}

// Desenha o Fantasma estilo "lençol"
void desenharFantasma(int x, int y, uint16_t cor) {
  if (imortal) cor = AZUL; // Fica azul se o pacman estiver forte
  tft.fillCircle(x + 8, y + 6, 6, cor);
  tft.fillRect(x + 2, y + 6, 12, 8, cor);
  desenharOlhos(x, y);
}

// Desenha o Pac-Man com olhos e boca animada
void desenharPacman(int x, int y, int dX, int dY, int r = 7) {
  uint16_t cor = imortal ? CIANO : AMARELO;
  tft.fillCircle(x + 8, y + 8, r, cor);
  if(r > 3) desenharOlhos(x, y); // Só desenha olhos se tiver tamanho
  
  // Lógica da boca (abre/fecha conforme movimento e tempo)
  if ((millis() / 150) % 2 == 0 && (dX != 0 || dY != 0)) {
    int boca = r + 1;
    if(dX > 0) tft.fillTriangle(x+8, y+8, x+8+boca, y+8-boca, x+8+boca, y+8+boca, PRETO);
    else if(dX < 0) tft.fillTriangle(x+8, y+8, x+8-boca, y+8-boca, x+8-boca, y+8+boca, PRETO);
    else if(dY > 0) tft.fillTriangle(x+8, y+8, x+8-boca, y+8+boca, x+8+boca, y+8+boca, PRETO);
    else if(dY < 0) tft.fillTriangle(x+8, y+8, x+8-boca, y+8-boca, x+8+boca, y+8-boca, PRETO);
  }
}

// Verifica se um ponto é parede
bool eParede(int x, int y) {
  if (x < 0 || x >= 160 || y < 0 || y >= 128) return true;
  return (mapa[y / TAM_BLOCO][x / TAM_BLOCO] == 1);
}

// Colisão robusta (Hitbox) para evitar que o pacman prenda nas quinas
bool colidindo(int x, int y) {
  int f = 3; // Folga de pixels
  if (eParede(x+f, y+f) || eParede(x+15-f, y+f) || eParede(x+f, y+15-f) || eParede(x+15-f, y+15-f)) return true;
  return false;
}

// Animação de Vitória: Pacman engole a tela e toca Despacito
void animacaoVitoria() {
  tft.fillScreen(PRETO);
  int r = 10;
  while(r < 150) {
    tft.fillCircle(80, 64, r, AMARELO);
    if((r/20)%2==0) tft.fillTriangle(80, 64, 80+r, 64-r, 80+r, 64+r, PRETO);
    r += 5;
    delay(15);
  }
  tft.fillScreen(AMARELO);
  tft.setTextColor(PRETO); tft.setTextSize(2);
  tft.setCursor(35, 55); tft.print("YOU WIN!");
  tocarDespacito();
  delay(2000);
  ESP.restart();
}

// Configura o mapa e personagens de cada fase
void carregarFase(int f) {
  tft.fillScreen(PRETO);
  tft.setCursor(30, 55); tft.setTextColor(BRANCO); tft.setTextSize(2);
  if(f == 3) tft.print("FASE FINAL");
  else { tft.print("FASE "); tft.print(f); }
  
  somInicioFase(); // Toca trombeta
  delay(1000);
  
  tft.fillScreen(PRETO);
  pacX = 16; pacY = 16; dirX = 0; dirY = 0; proxDirX = 0; proxDirY = 0;
  imortal = false;

  // Definição dos 3 Labirintos
  byte layouts[3][8][10] = {
    {{1,1,1,1,1,1,1,1,1,1},{1,3,0,0,0,0,0,0,3,1},{1,0,1,1,0,1,1,0,0,1},{1,0,1,0,0,0,1,0,0,1},{1,0,0,0,1,0,0,0,0,1},{1,0,1,1,1,1,1,1,0,1},{1,3,0,0,0,0,0,0,3,1},{1,1,1,1,1,1,1,1,1,1}},
    {{1,1,1,1,1,1,1,1,1,1},{1,3,0,0,1,1,0,0,3,1},{1,1,1,0,0,0,0,1,1,1},{1,0,0,0,1,1,0,0,0,1},{1,0,1,0,0,0,0,1,0,1},{1,1,1,0,1,1,0,1,1,1},{1,3,0,0,0,0,0,0,3,1},{1,1,1,1,1,1,1,1,1,1}},
    {{1,1,1,1,1,1,1,1,1,1},{1,3,1,0,0,0,0,1,3,1},{1,0,1,0,1,1,0,1,0,1},{1,0,0,0,0,0,0,0,0,1},{1,0,1,1,0,0,1,1,0,1},{1,0,0,0,1,1,0,0,0,1},{1,3,1,0,0,0,0,1,3,1},{1,1,1,1,1,1,1,1,1,1}}
  };

  pastilhasRestantes = 0;
  for(int i=0; i<8; i++) for(int j=0; j<10; j++) {
    mapa[i][j] = layouts[f-1][i][j];
    if(mapa[i][j] == 0 || mapa[i][j] == 3) pastilhasRestantes++;
  }

  // Ativa fantasmas conforme a fase
  for(int i=0; i<4; i++) {
    fantasmas[i].ativo = (i < f + 1); fantasmas[i].vivo = true;
    fantasmas[i].x = 80; fantasmas[i].y = 64; 
    fantasmas[i].dx = (i%2==0?0.5:-0.5); fantasmas[i].dy = 0;
    fantasmas[i].cor = (i==0?VERMELHO:(i==1?ROSA:(i==2?CIANO:0xFD20)));
  }
}

void setup() {
  // Inicialização de Pinos
  pinMode(PIN_UP, INPUT_PULLUP); pinMode(PIN_DOWN, INPUT_PULLUP);
  pinMode(PIN_LEFT, INPUT_PULLUP); pinMode(PIN_RIGHT, INPUT_PULLUP);
  pinMode(PIN_FIRE, INPUT_PULLUP); pinMode(PIN_BUZZER, OUTPUT);
  
  // Inicialização SPI e Display
  SPI.begin(TFT_SCK, -1, TFT_SDA, TFT_CS);
  tft.initR(INITR_BLACKTAB); 
  tft.setRotation(3); // Inverte 180 graus conforme solicitado
  tft.fillScreen(PRETO);
}

void loop() {
  unsigned long t = millis();

  if (gameState == 0) { // --- TELA INICIAL ---
    static int animX = -120;
    tft.fillScreen(PRETO);
    tft.setTextColor(AMARELO); tft.setTextSize(3);
    tft.setCursor(15, 20); tft.print("PAC-MAN");
    
    // Animação de introdução
    animX += 4; if(animX > 220) animX = -120;
    desenharPacman(animX, 70, 1, 0); 
    desenharFantasma(animX + 30, 70, VERMELHO);
    desenharFantasma(animX + 55, 70, ROSA);
    desenharFantasma(animX + 80, 70, CIANO);
    
    tft.setTextSize(1); tft.setTextColor(BRANCO);
    if((t/400)%2 == 0) { tft.setCursor(10, 110); tft.print("PRESS ANY BUTTON TO START"); }
    
    // Inicia se qualquer botão for pressionado
    if (digitalRead(PIN_UP)==LOW || digitalRead(PIN_DOWN)==LOW || digitalRead(PIN_LEFT)==LOW || digitalRead(PIN_RIGHT)==LOW || digitalRead(PIN_FIRE)==LOW) {
      fase = 1; carregarFase(fase); gameState = 1;
    }
    delay(40);
  } 
  else if (gameState == 1) { // --- JOGO ATIVO ---
    if (imortal && t - tempoImortal > 7000) imortal = false;

    // Leitura de comandos (Salva no buffer proxDir)
    if (digitalRead(PIN_UP) == LOW) { proxDirX = 0; proxDirY = -1; }
    else if (digitalRead(PIN_DOWN) == LOW) { proxDirX = 0; proxDirY = 1; }
    else if (digitalRead(PIN_LEFT) == LOW) { proxDirX = -1; proxDirY = 0; }
    else if (digitalRead(PIN_RIGHT) == LOW) { proxDirX = 1; proxDirY = 0; }

    // Limpeza de rastro
    tft.fillCircle(pacX + 8, pacY + 8, 8, PRETO);
    for(int i=0; i<4; i++) if(fantasmas[i].ativo && fantasmas[i].vivo) tft.fillRect(fantasmas[i].x, fantasmas[i].y, 16, 16, PRETO);

    // Sistema de Direção Automática (Snap to Grid)
    if (pacX % 16 == 0 && pacY % 16 == 0) {
      if (!colidindo(pacX + proxDirX, pacY + proxDirY)) { dirX = proxDirX; dirY = proxDirY; }
    }
    
    // Movimentação efetiva
    if (!colidindo(pacX + dirX, pacY + dirY)) { 
      pacX += dirX; pacY += dirY; 
    } else { 
      // Se bater, centraliza no bloco para não travar
      if(dirX != 0) pacX = round(pacX/16.0)*16; 
      if(dirY != 0) pacY = round(pacY/16.0)*16; 
      dirX = 0; dirY = 0; 
    }

    // Lógica de comer pastilhas
    int gx = (pacX+8)/16, gy = (pacY+8)/16;
    if (gx>=0 && gx<10 && gy>=0 && gy<8) {
      if (mapa[gy][gx] == 0 || mapa[gy][gx] == 3) {
        if(mapa[gy][gx] == 3) { imortal = true; tempoImortal = t; }
        mapa[gy][gx] = 2; pastilhasRestantes--; score += 10;
        tone(PIN_BUZZER, 1000, 5);
        if (pastilhasRestantes <= 0) {
          if (fase >= 3) { gameState = 4; animacaoVitoria(); } 
          else { fase++; carregarFase(fase); }
        }
      }
    }

    // Desenho do Labirinto
    for(int i=0; i<8; i++) for(int j=0; j<10; j++) {
      if(mapa[i][j] == 1) tft.drawRect(j*16, i*16, 16, 16, AZUL);
      else if(mapa[i][j] == 0) tft.fillCircle(j*16+8, i*16+8, 1, BRANCO);
      else if(mapa[i][j] == 3) tft.fillCircle(j*16+8, i*16+8, 3, BRANCO);
    }

    // Lógica dos Fantasmas
    for(int i=0; i<4; i++) {
      if(!fantasmas[i].ativo) continue;
      if(!fantasmas[i].vivo) {
        if(t - fantasmas[i].tempoRenascer > 10000) { fantasmas[i].vivo = true; fantasmas[i].x = 80; fantasmas[i].y = 64; }
        continue;
      }
      if(t % 3 == 0) { // Movem mais devagar que o Pacman
        float ox = fantasmas[i].x, oy = fantasmas[i].y;
        fantasmas[i].x += fantasmas[i].dx; fantasmas[i].y += fantasmas[i].dy;
        if(colidindo(fantasmas[i].x, fantasmas[i].y)) {
           fantasmas[i].x = ox; fantasmas[i].y = oy;
           int r = random(4);
           fantasmas[i].dx = (r==0?0.5:(r==1?-0.5:0)); fantasmas[i].dy = (r==2?0.5:(r==3?-0.5:0));
        }
      }
      desenharFantasma(fantasmas[i].x, fantasmas[i].y, fantasmas[i].cor);
      
      // Colisão Fantasma vs Pacman
      if(abs(pacX - (int)fantasmas[i].x) < 10 && abs(pacY - (int)fantasmas[i].y) < 10) {
        if(imortal) { 
          fantasmas[i].vivo = false; fantasmas[i].tempoRenascer = t; 
          tone(PIN_BUZZER, 2000, 20); 
        } else { 
          gameState = 3; // Game Over
        }
      }
    }
    desenharPacman(pacX, pacY, dirX, dirY);
    delay(10);
  }
  else if (gameState == 3) { // --- TELA DE DERROTA ---
    tft.fillScreen(PRETO); tft.setCursor(20, 60); tft.setTextColor(VERMELHO);
    tft.setTextSize(2); tft.print("GAME OVER");
    tone(PIN_BUZZER, 150, 500);
    delay(3000); ESP.restart();
  }
}