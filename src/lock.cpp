#include "kitezh/lock.hpp"

#include <stdexcept>

namespace kitezh {

Lock::Lock() {
    level_ = Level::Low;
    lower_gateState_ = GateState::Closed;
    upper_gateState_ = GateState::Closed;
    pumpCycles_ = 0;
}

Level Lock::level() const {
    return level_;
}

GateState Lock::gate(Gate gate) const {
    if (gate == Gate::Lower) {
        return lower_gateState_;
    }

    return upper_gateState_;

}

int Lock::pumpCycles() const {
    return pumpCycles_;
}

std::vector<Vessel> Lock::inside() const {
    return inside_;
}

void Lock::open(Gate gate) {
    if (gate == Gate::Lower) {
        if (lower_gateState_ == GateState::Open) {
            return;
        }
        if (level_ != Level::Low || upper_gateState_ == GateState::Open) {
            throw LockError("Нельзя открыть нижний затвор");
        }
        lower_gateState_ = GateState::Open;
    } else {
        if (upper_gateState_ == GateState::Open) {
            return;
        }
        if (level_ != Level::High  || lower_gateState_ == GateState::Open) {
            throw LockError("Нелья открыть верхний затвор");
        }
        upper_gateState_ = GateState::Open;
    }
}

void Lock::close(Gate gate) {
    if (gate == Gate::Lower) {
        lower_gateState_ = GateState::Closed;
    } else {
        upper_gateState_ = GateState::Closed;
    }

}

void Lock::pump() {
    if (lower_gateState_ == GateState::Open || upper_gateState_ == GateState::Open) {
        throw LockError("Затворы должны быть закрыты");
    }
    Level new_level = Level::High;
    int new_depth = kHighDepthDm;
    if (level_ == Level::High) {
        new_level = Level::Low;
        new_depth = kLowDepthDm;
    }

    for (const auto& v : inside_) {
        if (v.draft_dm > new_depth) {
            throw LockError("Осадка судна превышает глубину нового уровня");
        }
    }

    level_ = new_level;
    pumpCycles_ += 1;
}

void Lock::enter(const Vessel& vessel) {
    if (validateVessel(vessel).has_value()) {
        throw LockError("Некорректные данные судна");
    }

    for (const auto& v : inside_) {
        if (v.call_sign == vessel.call_sign) {
            throw LockError("Судно с таким позывным уже находится в камере");
        }
    }

    int current_depth = kLowDepthDm;
    if (level_ == Level::High) {
        current_depth = kHighDepthDm;
    }
    if (vessel.draft_dm > current_depth) {
        throw LockError("Осадка судна больше текущей глубины");
    }

    if (vessel.direction == Direction::Upstream && lower_gateState_ != GateState::Open) {
        throw LockError("Для входа вверх должен быть открыт нижний затвор");
    }
    if (vessel.direction == Direction::Downstream && upper_gateState_ != GateState::Open) {
        throw LockError("Для входа вниз должен быть открыт верхний затвор");
    }

    if (vessel.kind == VesselKind::Oversized && !inside_.empty()) {
        throw LockError("Негабаритное судно может заходить только в пустую камеру");
    }
    for (const auto& v : inside_) {
        if (v.kind == VesselKind::Oversized) {
            throw LockError("Нельзя заходить в камеру с негабаритным судном");
        }
    }

    if (!fitsInChamber(inside_, vessel)) {
        throw LockError("Судно не помещается в камеру по длине");
    }

    inside_.push_back(vessel);
}

void Lock::exit(const std::string& call_sign) {
    if (inside_.empty()) {
        throw LockError("Камера пуста");
    }

    const Vessel& head = inside_.front();
    if (head.call_sign != call_sign) {
        throw LockError("Выйти может только первое судно в камере");
    }

    if (head.direction == Direction::Upstream && upper_gateState_ != GateState::Open) {
        throw LockError("Для выхода вверх должен быть открыт верхний затвор");
    }
    if (head.direction == Direction::Downstream && lower_gateState_ != GateState::Open) {
        throw LockError("Для выхода вниз должен быть открыт нижний затвор");
    }

    inside_.erase(inside_.begin());
}

}  // namespace kitezh
