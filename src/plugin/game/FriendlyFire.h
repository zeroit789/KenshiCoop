// FriendlyFire.h - lógica PURA de mitigación de fuego amigo en coop.
//
// Problema (coop-específico, NO un fix general de fuego amigo contra el mundo):
// el cuerpo del OTRO jugador (y sus NPCs reclutados) se representa en el cliente
// local como un "proxy" metido en una facción de mundo prestada. Si un jugador
// roza accidentalmente a un compañero de su propio grupo de coop (un ataque de
// área, un mal swing en pleno combate cooperativo...), el motor dispara el
// evento de facción ATTACKED_US_* y declara hostilidad/guerra contra esa facción
// prestada. Entre miembros del MISMO grupo de coop ese golpe no debe contar como
// agresión de facción.
//
// Este header aísla la ÚNICA decisión testeable ("¿este golpe es fuego amigo
// entre dos cuerpos del propio grupo de coop que debo suprimir?") SIN ninguna
// dependencia del motor/KenshiLib. Por eso prototest.exe (que solo enlaza el
// CRT) puede incluirlo y ejercitar la decisión de forma aislada, igual que hace
// con EngineFaults.h / EngineCaps.h. El pegamento con el motor (leer facción del
// personaje, consultar los sets de proxies, armar el bracket alrededor del
// golpe) vive en EngineInternal.cpp; aquí solo va la lógica combinatoria pura.
#pragma once

namespace coop {
namespace ff {

// Los dos eventos "nos atacaron" del enum FactionRelations::FactionEvent
// (ATTACKED_US_DEFENSIVELY = 0, ATTACKED_US_AGGRESSIVELY = 1). Se duplican aquí
// como constantes libres para no arrastrar el header del motor a prototest; el
// static_assert de EngineInternal.cpp ancla que estos valores siguen casando con
// el enum real de KenshiLib (si Lo-Fi reordena el enum, ese assert salta).
enum {
    EV_ATTACKED_US_DEFENSIVELY  = 0,
    EV_ATTACKED_US_AGGRESSIVELY = 1
};

// ¿Es 'event' uno de los ATTACKED_US_*? Son los únicos eventos que un golpe de
// combate dispara para empeorar la relación de facción ("me atacaste"). El resto
// del enum (DEFEATED/KILLED/CAPTURED/AIDED/...) queda deliberadamente fuera: solo
// nos interesa neutralizar la agresión POR EL GOLPE en sí.
inline bool isAttackedUsEvent(int event) {
    return event == EV_ATTACKED_US_DEFENSIVELY ||
           event == EV_ATTACKED_US_AGGRESSIVELY;
}

// Decisión pura: ¿debe suprimirse este evento de facción por tratarse de fuego
// amigo entre dos miembros del MISMO grupo de coop?
//
// Se suprime solo cuando se cumplen las TRES condiciones:
//   1. El evento es un ATTACKED_US_* (isAttackedUsEvent).
//   2. El ATACANTE es un cuerpo controlado por un jugador del coop (squad local o
//      proxy del compañero).
//   3. La VÍCTIMA también es un cuerpo controlado por un jugador del coop.
//
// Un golpe contra un NPC real del mundo deja victimIsCoopBody = false y NO se
// suprime: el fuego amigo/agresión contra el mundo sigue funcionando igual que en
// vanilla (intencionalmente fuera de alcance). Del mismo modo, un proxy del
// compañero atacando a un NPC del mundo deja victimIsCoopBody = false y tampoco
// se suprime.
//
// No se distingue ataque intencional de accidental: Kenshi no expone esa
// intención a nivel de evento de facción, y entre compañeros del propio squad
// ninguno de los dos casos debe declarar guerra, así que ambos se tratan igual.
inline bool shouldSuppressAttackAsFriendlyFire(int event,
                                               bool attackerIsCoopBody,
                                               bool victimIsCoopBody) {
    return isAttackedUsEvent(event) && attackerIsCoopBody && victimIsCoopBody;
}

} // namespace ff
} // namespace coop
