#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

class $modify(FastAutoCheckpointPlayLayer, PlayLayer) {
    struct Fields {
        float m_checkpointTimer = 0.0f;
        float m_cooldownTimer = 0.0f;
        float m_timeSinceRespawn = 0.0f;
        float m_timeInAir = 0.0f;
        float m_deletePauseTimer = 0.0f;
        bool m_wasGrounded = false;
        bool m_wasDashing = false;
        bool m_wasRingJump = false;
        bool m_wasPad = false;
        bool m_wasUpsideDown = false;
        bool m_placedFirstCheckpoint = false;
        int m_lastVehicleType = 0; // 0: Cubo, 1: Nave, 2: Ball, 3: UFO, 4: Wave, 5: Robot, 6: Spider, 7: Swing
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }

        log::info("[SmartAutoCheckpoint] SoyRiper - PlayLayer::init v2.2.0 cargado en nivel: {}", level ? level->m_levelName.c_str() : "Desconocido");

        m_fields->m_checkpointTimer = 0.0f;
        m_fields->m_cooldownTimer = 1.0f;
        m_fields->m_timeSinceRespawn = 0.0f;
        m_fields->m_timeInAir = 0.0f;
        m_fields->m_deletePauseTimer = 0.0f;
        m_fields->m_wasGrounded = false;
        m_fields->m_wasDashing = false;
        m_fields->m_wasRingJump = false;
        m_fields->m_wasPad = false;
        m_fields->m_wasUpsideDown = false;
        m_fields->m_placedFirstCheckpoint = false;
        m_fields->m_lastVehicleType = 0;

        return true;
    }

    void resetLevel() {
        PlayLayer::resetLevel();

        log::info("[SmartAutoCheckpoint] resetLevel ejecutado (respawn).");

        m_fields->m_checkpointTimer = 0.0f;
        m_fields->m_cooldownTimer = 1.0f;
        m_fields->m_timeSinceRespawn = 0.0f;
        m_fields->m_timeInAir = 0.0f;
        m_fields->m_deletePauseTimer = 0.0f;
        m_fields->m_wasGrounded = false;
        m_fields->m_wasDashing = false;
        m_fields->m_wasRingJump = false;
        m_fields->m_wasPad = false;
        m_fields->m_wasUpsideDown = false;
        m_fields->m_placedFirstCheckpoint = false;
        m_fields->m_lastVehicleType = 0;
    }

    void removeCheckpoint(bool first) {
        PlayLayer::removeCheckpoint(first);
        bool pauseOnDelete = Mod::get()->getSettingValue<bool>("pause-on-delete");
        if (pauseOnDelete) {
            m_fields->m_deletePauseTimer = 2.0f; // Pausa la colocación por 2 segundos tras borrar con X
            log::info("[SmartAutoCheckpoint] Checkpoint borrado con X. Mod pausado por 2.0s.");
        }
    }

    int getCurrentVehicleType() {
        if (!m_player1) return 0;
        if (m_player1->m_isShip) return 1;
        if (m_player1->m_isBall) return 2;
        if (m_player1->m_isBird) return 3;
        if (m_player1->m_isDart) return 4;
        if (m_player1->m_isRobot) return 5;
        if (m_player1->m_isSpider) return 6;
        if (m_player1->m_isSwing) return 7;
        return 0; // Cubo
    }

    void executeCheckpoint(const char* reason) {
        log::info("[SmartAutoCheckpoint] Checkpoint Clave (Motivo: {})...", reason);
        
        CheckpointObject* cp = this->markCheckpoint();
        if (!cp) {
            cp = this->createCheckpoint();
            if (cp) {
                this->storeCheckpoint(cp);
            }
        }
        if (cp) {
            log::info("[SmartAutoCheckpoint] CHECKPOINT COLOCADO: ptr={}", static_cast<void*>(cp));
        } else {
            log::warn("[SmartAutoCheckpoint] markCheckpoint y createCheckpoint devolvieron NULL.");
        }
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        // Solo actuar en Modo Práctica
        if (!m_isPracticeMode) {
            return;
        }

        // Verificar si el mod está activado
        bool enabled = Mod::get()->getSettingValue<bool>("enabled");
        if (!enabled) {
            return;
        }

        // Verificar si el jugador existe y está vivo
        if (!m_player1 || m_player1->m_isDead) {
            return;
        }

        bool isGrounded = (m_player1->m_isOnGround || m_player1->m_isOnSlope);
        bool isDashing = m_player1->m_isDashing;
        bool isRingJump = (m_player1->m_touchedRing || m_player1->m_stateRingJump);
        bool isPad = m_player1->m_touchedPad;
        bool isUpsideDown = m_player1->m_isUpsideDown;
        int currentVehicle = getCurrentVehicleType();

        // 1. PAUSA AL BORRAR CON X (Si acabas de presionar X, pausar 2s para borrar sin molestias)
        if (m_fields->m_deletePauseTimer > 0.0f) {
            m_fields->m_deletePauseTimer -= dt;
            m_fields->m_wasGrounded = isGrounded;
            m_fields->m_wasDashing = isDashing;
            m_fields->m_wasRingJump = isRingJump;
            m_fields->m_wasPad = isPad;
            m_fields->m_wasUpsideDown = isUpsideDown;
            m_fields->m_lastVehicleType = currentVehicle;
            return;
        }

        m_fields->m_timeSinceRespawn += dt;
        m_fields->m_cooldownTimer += dt;

        float safetyDelay = static_cast<float>(Mod::get()->getSettingValue<double>("safety-delay"));
        if (m_fields->m_timeSinceRespawn < safetyDelay) {
            m_fields->m_wasGrounded = isGrounded;
            m_fields->m_wasDashing = isDashing;
            m_fields->m_wasRingJump = isRingJump;
            m_fields->m_wasPad = isPad;
            m_fields->m_wasUpsideDown = isUpsideDown;
            m_fields->m_lastVehicleType = currentVehicle;
            return;
        }

        // Medir tiempo continuo en el aire (evita spam al caminar por el piso)
        if (!isGrounded) {
            m_fields->m_timeInAir += dt;
        }

        bool triggerCheckpoint = false;
        const char* triggerReason = "";

        // 1. PRIMER CHECKPOINT TRAS RESPAWN (De inmediato tras el margen de seguridad de 0.10s)
        if (!m_fields->m_placedFirstCheckpoint) {
            triggerCheckpoint = true;
            triggerReason = "Primer Checkpoint tras Respawn";
            m_fields->m_placedFirstCheckpoint = true;
        }

        // 2. ATERRIZAJE EN PISO / PLATAFORMA SÓLIDA (Requiere haber estado al menos 0.35s en el aire para evitar spam al caminar)
        bool onGroundSetting = Mod::get()->getSettingValue<bool>("checkpoint-on-ground");
        if (!triggerCheckpoint && onGroundSetting && isGrounded && !m_fields->m_wasGrounded) {
            if (m_fields->m_timeInAir >= 0.35f) {
                triggerCheckpoint = true;
                triggerReason = "Aterrizó tras caída/salto real";
            }
        }

        // 3. ACTIVACIÓN DE ORBES, TRAMPOLINES O DASH RINGS (Evento Clave)
        bool onOrbsSetting = Mod::get()->getSettingValue<bool>("checkpoint-on-orbs");
        if (!triggerCheckpoint && onOrbsSetting) {
            if (isDashing && !m_fields->m_wasDashing) {
                triggerCheckpoint = true;
                triggerReason = "Activó Dash Ring";
            } else if (isRingJump && !m_fields->m_wasRingJump) {
                triggerCheckpoint = true;
                triggerReason = "Activó Orbe / Ring";
            } else if (isPad && !m_fields->m_wasPad) {
                triggerCheckpoint = true;
                triggerReason = "Tocó Jump Pad";
            }
        }

        // 4. ENTRADA A PORTAL DE VEHÍCULO O CAMBIO DE GRAVEDAD (Evento Clave)
        bool onPortalsSetting = Mod::get()->getSettingValue<bool>("checkpoint-on-portals");
        if (!triggerCheckpoint && onPortalsSetting) {
            if (currentVehicle != m_fields->m_lastVehicleType) {
                triggerCheckpoint = true;
                triggerReason = "Portal de vehículo";
            } else if (isUpsideDown != m_fields->m_wasUpsideDown) {
                triggerCheckpoint = true;
                triggerReason = "Cambio de gravedad";
            }
        }

        // Si se aterrizó en suelo, reiniciar tiempo en aire
        if (isGrounded) {
            m_fields->m_timeInAir = 0.0f;
        }

        // Guardar estado previo para el siguiente frame
        m_fields->m_wasGrounded = isGrounded;
        m_fields->m_wasDashing = isDashing;
        m_fields->m_wasRingJump = isRingJump;
        m_fields->m_wasPad = isPad;
        m_fields->m_wasUpsideDown = isUpsideDown;
        m_fields->m_lastVehicleType = currentVehicle;

        // SEPARACIÓN MÍNIMA OBLIGATORIA (Evita acumular checkpoints pegados)
        float minCooldown = static_cast<float>(Mod::get()->getSettingValue<double>("min-checkpoint-cooldown"));

        if (triggerCheckpoint && m_fields->m_cooldownTimer >= minCooldown) {
            m_fields->m_checkpointTimer = 0.0f;
            m_fields->m_cooldownTimer = 0.0f;
            executeCheckpoint(triggerReason);
        }
    }
};
