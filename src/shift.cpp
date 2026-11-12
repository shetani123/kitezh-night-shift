#include "kitezh/shift.hpp"

#include "kitezh/lock.hpp"

#include <stdexcept>
#include <unordered_set>

namespace kitezh {

namespace {

std::vector<Vessel> filterQueue(const std::vector<Vessel>& queue, ShiftReport& report) {
    std::vector<Vessel> waiting;
    std::unordered_set<std::string> seen_call_signs;

    for (const auto& vessel : queue) {
        if (seen_call_signs.count(vessel.call_sign) > 0) {
            report.refused.push_back(Refusal{vessel.call_sign, "duplicate"});
            continue;
        }
        seen_call_signs.insert(vessel.call_sign);

        if (validateVessel(vessel).has_value()) {
            report.refused.push_back(Refusal{vessel.call_sign, "invalid"});
            continue;
        }
        if (vessel.draft_dm > kLowDepthDm) {
            report.refused.push_back(Refusal{vessel.call_sign, "draft"});
            continue;
        }

        waiting.push_back(vessel);
    }

    return waiting;
}

std::vector<Vessel> takeNextBatch(std::vector<Vessel>& waiting) {
    std::vector<Vessel> batch;
    std::vector<Vessel> remaining;

    const Vessel& first = waiting[0];
    batch.push_back(first);

    if (first.kind == VesselKind::Oversized) {
        for (std::size_t i = 1; i < waiting.size(); ++i) {
            remaining.push_back(waiting[i]);
        }
    } else {
        Direction dir = first.direction;
        bool skipped_someone = false;

        for (std::size_t i = 1; i < waiting.size(); ++i) {
            const Vessel& candidate = waiting[i];
            bool can_take = candidate.direction == dir
                && candidate.kind != VesselKind::Oversized
                && fitsInChamber(batch, candidate)
                && (!skipped_someone || candidate.kind == VesselKind::Ambulance);

            if (can_take) {
                batch.push_back(candidate);
            } else {
                skipped_someone = true;
                remaining.push_back(candidate);
            }
        }
    }

    waiting = std::move(remaining);
    return batch;
}

void passBatch(Lock& lock, const std::vector<Vessel>& batch, ShiftReport& report) {
    Level entry_level = Level::Low;
    Gate entry_gate = Gate::Lower;
    Gate exit_gate = Gate::Upper;

    if (batch[0].direction == Direction::Downstream) {
        entry_level = Level::High;
        entry_gate = Gate::Upper;
        exit_gate = Gate::Lower;
    }

    if (lock.level() != entry_level) {
        lock.pump();
    }

    lock.open(entry_gate);
    for (const auto& vessel : batch) {
        lock.enter(vessel);
    }
    lock.close(entry_gate);

    lock.pump();

    lock.open(exit_gate);
    for (const auto& vessel : batch) {
        lock.exit(vessel.call_sign);
        report.passed.push_back(vessel.call_sign);
    }
    lock.close(exit_gate);
}

}  // namespace

ShiftReport runNightShift(const std::vector<Vessel>& queue) {
    ShiftReport report;
    Lock lock;
    std::vector<Vessel> waiting = filterQueue(queue, report);

    while (!waiting.empty()) {
        std::vector<Vessel> batch = takeNextBatch(waiting);
        passBatch(lock, batch, report);
    }

    report.pump_cycles = lock.pumpCycles();
    return report;
}

}  // namespace kitezh
