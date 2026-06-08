# ⚡ OrbitAlert — Edge Computing
### Global Solution 2026 · FIAP · 1ESW
**Disciplina:** Edge Computing & Computer Systems  
**Aluno:** Thiago Kulesza · RM 568922  
**Turma:** 1ESPG  

---

## 📡 Sobre o Projeto

O **OrbitAlert** é uma plataforma de monitoramento de clima espacial que alerta sobre tempestades solares e seus riscos para infraestrutura terrestre no Brasil — redes elétricas, GPS, satélites e telecomunicações.

Este módulo é o nó de **Edge Computing**: um protótipo simulado no **Wokwi** que detecta variações de radiação solar e emite alertas locais em tempo real, sem depender de conexão com a internet.

> **Base técnica:** arquitetura herdada do **CP2 – Vinheria Agnello** (Debuggers), com média móvel de leituras, menu LCD navegável por botões e buzzer intermitente — adaptada para o contexto de clima espacial.

---

## 🛠️ Componentes do Circuito

| Componente | Pino Arduino | Função |
|---|---|---|
| **LDR** + resistor **10kΩ** | `A0` | Divisor de tensão — simula intensidade de radiação solar |
| **LED Vermelho** + resistor **220Ω** | `D13` | Alerta visual — pisca com frequência proporcional ao risco |
| **Buzzer passivo** | `D12` | Alerta sonoro — bipa em ALTO e CRÍTICO |
| **LCD 16x2 I2C** | `SDA (A4)` / `SCL (A5)` | Exibe 4 telas navegáveis |
| **Botão MENU** | `D7` | Avança para próxima tela |
| **Botão UP** | `D6` | Avança tela |
| **Botão DOWN** | `D5` | Volta tela |

---

## 🔌 Esquema de Conexão

```
Arduino Uno
│
├── A0  ──── LDR:AO ──── nó ──── Resistor 10kΩ ──── GND
│                    └── (mesmo nó) leitura analógica
│
├── A4  ──── LCD:SDA     LCD:VCC ──── 5V
├── A5  ──── LCD:SCL     LCD:GND ──── GND
│
├── D13 ──── Resistor 220Ω ──── LED:A (anodo)
│                               LED:C (catodo) ──── GND
│
├── D12 ──── Buzzer:2 (positivo)
│            Buzzer:1 (negativo) ──── GND
│
├── D7  ──── Botão MENU ──── GND  (INPUT_PULLUP)
├── D6  ──── Botão UP   ──── GND  (INPUT_PULLUP)
└── D5  ──── Botão DOWN ──── GND  (INPUT_PULLUP)
```

---

## 📊 Classificação de Risco

| ADC (0–1023) | % | Nível | Escala Kp/G | LED | Buzzer |
|---|---|---|---|---|---|
| 0 – 249 | 0–24% | 🟢 **BAIXO** | G0–G3 | Apagado | Silencioso |
| 250 – 499 | 25–49% | 🟡 **MEDIO** | G4–G5 | Pisca 1000ms | Silencioso |
| 500 – 749 | 50–73% | 🟠 **ALTO** | G6–G7 | Pisca 400ms | Bip 1200Hz/800ms |
| 750 – 1023 | 74–100% | 🔴 **CRITICO** | G8–G9 | Pisca 150ms | Bip 2000Hz/300ms |

> **Por que LDR?** O LDR simula a intensidade de radiação solar de forma didática: mais luz = maior radiação simulada = maior risco. Em produção real, a leitura viria da **NASA DONKI API** via ESP32 com Wi-Fi.

---

## 🖥️ Telas do LCD (navegáveis por botão)

**Tela 0 — Risco Atual**
```
⚡ RISCO:CRITICO
[##########]98%
```

**Tela 1 — Barra LDR**
```
LDR:812 (79%)
[############--]
```

**Tela 2 — Índice Kp**
```
Kp SIMULADO:
7.2  [ALTO   ]
```

**Tela 3 — Sobre**
```
OrbitAlert v1.0
FIAP GS26 1ESW
```

---

## 🔁 Fluxo do Sistema

```
┌──────────────────────────────────────────────────────┐
│                   LOOP PRINCIPAL                     │
│                                                      │
│  A cada 200ms:                                       │
│    1. lerLDR() → média móvel 10 amostras             │
│    2. calcularNivel() → 0, 1, 2 ou 3                 │
│    3. Se nível mudou → atualizarLCD() (tela ativa)   │
│    4. Log Serial (ADC, %, nível, Kp simulado)        │
│                                                      │
│  A cada iteração (50ms):                             │
│    5. verificarBotoes() → troca tela se pressionado  │
│    6. gerenciarLED() → apagado ou piscando           │
│    7. gerenciarBuzzer() → silêncio ou tone()         │
└──────────────────────────────────────────────────────┘
```

**Média móvel (herança CP2):** buffer circular de 10 amostras evita oscilações de display causadas por ruído elétrico no LDR.

---

## 🚀 Como Simular no Wokwi

1. Acesse [wokwi.com](https://wokwi.com/projects/465264724135139329)
2. Cole `OrbitAlert_sketch.ino` no editor
3. **Add Library** → busque `LiquidCrystal I2C` → instale
4. Clique em **`{}`** → cole `OrbitAlert_diagram.json`
5. **▶ Start Simulation**
6. Arraste o **slider do LDR** para simular diferentes intensidades solares
7. Use os **botões MENU/UP/DOWN** para navegar pelas telas

| Slider LDR | Nível esperado |
|---|---|
| Mínimo | 🟢 BAIXO |
| ~25% | 🟡 MEDIO |
| ~50% | 🟠 ALTO |
| Máximo | 🔴 CRITICO |

---

## 📁 Estrutura de Arquivos

```
edge-computing/
├── OrbitAlert_sketch.ino    # Código-fonte Arduino
├── OrbitAlert_diagram.json  # Diagrama Wokwi
└── README.md                # Este documento
```

---

## 🔗 Integração com o Projeto Geral

```
NASA DONKI API
      │
      ▼
  Back-end Python          Front-End Web
  (Guilherme)              (Enrico)
      │                        │
      └──────────┬─────────────┘
                 ▼
       Arduino Uno — este módulo
       Nó de alerta local offline
       LDR + LCD + LED + Buzzer + Botões
```

---

## 👥 Equipe OrbitAlert · Debuggers

| Nome | RM | Disciplina | Entrega |
|---|---|---|---|
| Enrico Vieira de Almeida Vidal | 569217 | Front-End + Web Dev | Landing page + GitHub Org |
| Vinícius Fuentes Cavalcanti | 570818 | Software & UX | PDF: Backlog, User Flow |
| Thiago Fernandes Kulesza | 568922 | Edge Computing | Circuito Wokwi + README |
| Guilherme De Rosa Peres | 569193 | Python + Matemática + Pitch | Menu Python + gráficos + vídeo |

---

## 📚 Referências

- [NASA DONKI API](https://api.nasa.gov/) — Heliophysics Event Catalog
- [NOAA Space Weather Scales](https://www.swpc.noaa.gov/noaa-scales-explanation) — Escala Kp/G
- [Wokwi Simulator](https://wokwi.com) — Simulação Arduino online
- [LiquidCrystal_I2C](https://github.com/johnrickman/LiquidCrystal_I2C) — Biblioteca LCD I2C

---

*OrbitAlert · Global Solution 2026 · FIAP · 1ESPG-26*
