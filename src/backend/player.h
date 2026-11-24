#pragma once

#include "messages.h"

// Estado do jogador + atualizacao noclip. Roda somente na thread do backend.
class Player {
public:
    float px = 0.0f, py = 120.0f, pz = 0.0f;
    float yaw = 0.0f;   // graus
    float pitch = 0.0f; // graus, [-89, 89]

    float speed        = 12.0f; // blocos/s
    float shift_mult   = 3.0f;
    float sensitivity  = 0.1f;  // graus por pixel

    // Atualiza posicao e orientacao a partir do input e do dt do tick.
    void update(const InputState& in, float dt);

    // Preenche os campos de posicao/orientacao no snapshot.
    void fill_snapshot(PlayerSnapshot& s) const;
};