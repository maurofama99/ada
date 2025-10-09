
#ifndef FSA_H
#define FSA_H

#include <iostream>
#include <map>
#include <vector>
#include <unordered_map>
#include <unordered_set>

class FiniteStateAutomaton {
public:
    struct Transition {
        long long fromState;
        long long toState;
        long long label;
    };
    
    std::unordered_map<long long, std::vector<Transition> > transitions;
    long long initialState;
    std::unordered_set<long long> finalStates;
    
    FiniteStateAutomaton() : initialState(0) {}
    
    void addTransition(long long fromState, long long toState, long long label) {
        transitions[fromState].push_back((Transition){fromState, toState, label});
    }
    
    void addFinalState(long long state) {
        finalStates.insert(state);
    }
    
    [[nodiscard]] std::vector<std::pair<long long, long long> > getStatePairsWithTransition(long long label) const {
        std::vector<std::pair<long long, long long> > statePairs;
        for (const auto& pair : transitions) {
            for (const auto& transition : pair.second) {
                if (transition.label == label) {
                    statePairs.emplace_back(transition.fromState, transition.toState);
                }
            }
        }
        return statePairs;
    }
    
    long long getNextState(const long long currentState, const long long label) {
        for (const auto& transition : transitions[currentState]) {
            if (transition.label == label) {
                return transition.toState;
            }
        }
        return -1; // No valid transition
    }
    
    std::map<long long, long long> getAllSuccessors(const long long state) {
        std::map<long long, long long> successors;
        for (const auto& transition : transitions[state]) {
            successors[transition.label] = transition.toState;
        }
        return successors;
    }

    [[nodiscard]] bool hasLabel(const long long label) const {
        for (const auto&[fst, snd] : transitions) {
            for (const auto& transition : snd) {
                if (transition.label == label) {
                    return true;
                }
            }
        }
        return false;
    }

    [[nodiscard]] bool isFinalState(const long long state) const {
        return finalStates.find(state) != finalStates.end();
    }
    
    void printTransitions() const {
        for (const auto& pair : transitions) {
            for (const auto& transition : pair.second) {
                std::cout << "From State " << transition.fromState << " to State " << transition.toState << " on Label '" << transition.label << "'\n";
            }
        }
    }
};

#endif //FSA_H
