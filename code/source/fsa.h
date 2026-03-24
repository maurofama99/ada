
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
    int states_count = 0;
    unsigned int scores[4]{};
    
    FiniteStateAutomaton(long long query_type, const std::vector<long long> &labels) : initialState(0) {
        switch (query_type) {
            case 1: // a+
                addFinalState(1);
                addTransition(0, 1, labels[0]);
                addTransition(1, 1, labels[0]);
                states_count = 2;
                scores[0] = 6;
                break;
            case 5: // ab*c
                addFinalState(2);
                addTransition(0, 1, labels[0]);
                addTransition(1, 1, labels[1]);
                addTransition(1, 2, labels[2]);
                states_count = 3;
                scores[0] = 0;
                scores[1] = 7;
                scores[2] = 0;
                break;
            case 7: // abc*
                addFinalState(2);
                addTransition(0, 1, labels[0]);
                addTransition(1, 2, labels[1]);
                addTransition(2, 2, labels[2]);
                states_count = 3;
                scores[0] = 0;
                scores[1] = 7;
                scores[2] = 6;
                break;
            case 4: // (abc)+
                addFinalState(3);
                addTransition(0, 1, labels[0]);
                addTransition(1, 2, labels[1]);
                addTransition(2, 3, labels[2]);
                addTransition(3, 1, labels[0]);
                scores[0] = 7;
                scores[1] = 6;
                scores[2] = 6;
                scores[3] = 6;
                states_count = 4;
                break;
            case 2: // ab*
                addFinalState(1);
                addTransition(0, 1, labels[0]);
                addTransition(1, 1, labels[1]);
                states_count = 2;
                scores[0] = 0;
                scores[1] = 6;
                break;
            case 10: // (a|b)c*
                addFinalState(1);
                addTransition(0, 1, labels[0]);
                addTransition(0, 1, labels[1]);
                addTransition(1, 1, labels[2]);
                states_count = 2;
                scores[0] = 7;
                scores[1] = 6;
                break;
            case 6: // a*b*
                addFinalState(1);
                addFinalState(2);
                addTransition(0, 1, labels[0]);
                addTransition(1, 1, labels[0]);
                addTransition(1, 2, labels[1]);
                addTransition(0, 2, labels[1]);
                addTransition(2, 2, labels[1]);
                states_count = 3;
                scores[0] = 13;
                scores[1] = 6;
                break;
            case 3: // ab*c*
                addFinalState(1);
                addFinalState(2);
                addTransition(0, 1, labels[0]);
                addTransition(1, 1, labels[1]);
                addTransition(1, 2, labels[2]);
                addTransition(2, 2, labels[2]);
                states_count = 3;
                scores[0] = 0;
                scores[1] = 13;  /// 2 loops, 1 edge 1->2, thus 2*6+1 = 13
                scores[2] = 6;
                break;
            default:
                std::cerr << "ERROR: Wrong query type" << std::endl;
                exit(1);
        }
    }
    
    void addTransition(long long fromState, long long toState, long long label) {
        transitions[fromState].push_back((Transition){fromState, toState, label});
    }
    
    void addFinalState(long long state) {
        finalStates.insert(state);
    }

    [[nodiscard]] std::vector<long long> getKleeneStarLabels() const {
        std::vector<long long> kleeneStarLabels;
        for (const auto& pair : transitions) {
            for (const auto& transition : pair.second) {
                if (transition.fromState == transition.toState) {
                    kleeneStarLabels.push_back(transition.label);
                }
            }
        }
        return kleeneStarLabels;
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
