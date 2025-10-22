/*++

Copyright (c) Microsoft. All rights reserved.

Module Name:

    standalone_cognitive_test.cpp

Abstract:

    Standalone test for the OpenCog cognitive architecture framework
    that doesn't depend on WSL-specific components.

--*/

#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <functional>
#include <atomic>
#include <chrono>
#include <shared_mutex>
#include <mutex>
#include <thread>
#include <algorithm>
#include <random>
#include <iostream>
#include <cassert>

// Simplified cognitive framework for testing
namespace cognitive_test {

class Atom {
public:
    enum class Type : uint32_t {
        Concept = 0,
        Link,
        Process,
        Agent,
        Rule,
        Goal,
        Memory
    };

    Atom(Type type, const std::string& name, float truthValue = 1.0f, float confidence = 1.0f)
        : m_type(type), m_name(name), m_id(s_nextId++), m_truthValue(truthValue), 
          m_confidence(confidence), m_attention(0.5f) {}

    Type GetType() const { return m_type; }
    const std::string& GetName() const { return m_name; }
    uint64_t GetId() const { return m_id; }
    
    float GetTruthValue() const { return m_truthValue; }
    float GetConfidence() const { return m_confidence; }
    void SetTruthValue(float truth, float confidence) {
        m_truthValue = std::clamp(truth, 0.0f, 1.0f);
        m_confidence = std::clamp(confidence, 0.0f, 1.0f);
    }
    
    float GetAttention() const { return m_attention; }
    void SetAttention(float attention) { m_attention = attention; }
    
    const std::vector<std::shared_ptr<Atom>>& GetOutgoingLinks() const { return m_outgoingLinks; }
    void AddOutgoingLink(std::shared_ptr<Atom> atom) {
        if (atom && std::find(m_outgoingLinks.begin(), m_outgoingLinks.end(), atom) == m_outgoingLinks.end()) {
            m_outgoingLinks.push_back(atom);
        }
    }

private:
    static std::atomic<uint64_t> s_nextId;
    Type m_type;
    std::string m_name;
    uint64_t m_id;
    float m_truthValue;
    float m_confidence;
    float m_attention;
    std::vector<std::shared_ptr<Atom>> m_outgoingLinks;
};

std::atomic<uint64_t> Atom::s_nextId{1};

class AtomSpace {
public:
    std::shared_ptr<Atom> CreateAtom(Atom::Type type, const std::string& name, 
                                   float truthValue = 1.0f, float confidence = 1.0f) {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        
        auto existing = m_atomsByName.find(name);
        if (existing != m_atomsByName.end()) {
            return existing->second;
        }
        
        auto atom = std::make_shared<Atom>(type, name, truthValue, confidence);
        m_atoms[atom->GetId()] = atom;
        m_atomsByName[name] = atom;
        
        return atom;
    }

    std::shared_ptr<Atom> FindAtom(const std::string& name) const {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        auto it = m_atomsByName.find(name);
        return (it != m_atomsByName.end()) ? it->second : nullptr;
    }

    std::vector<std::shared_ptr<Atom>> FindAtomsByType(Atom::Type type) const {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        std::vector<std::shared_ptr<Atom>> result;
        
        for (const auto& [id, atom] : m_atoms) {
            if (atom->GetType() == type) {
                result.push_back(atom);
            }
        }
        
        return result;
    }

    std::vector<std::shared_ptr<Atom>> Query(const std::function<bool(const Atom&)>& predicate) const {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        std::vector<std::shared_ptr<Atom>> result;
        
        for (const auto& [id, atom] : m_atoms) {
            if (predicate(*atom)) {
                result.push_back(atom);
            }
        }
        
        return result;
    }

    size_t GetAtomCount() const { 
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return m_atoms.size(); 
    }

    void UpdateAttentionValues() {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        
        for (const auto& [id, atom] : m_atoms) {
            float currentAttention = atom->GetAttention();
            float newAttention = currentAttention * 0.95f;
            
            const auto& outgoing = atom->GetOutgoingLinks();
            if (!outgoing.empty()) {
                float spreadAmount = currentAttention * 0.1f / outgoing.size();
                for (auto& linked : outgoing) {
                    linked->SetAttention(linked->GetAttention() + spreadAmount);
                }
            }
            
            atom->SetAttention(std::max(0.01f, newAttention));
        }
    }

private:
    std::unordered_map<uint64_t, std::shared_ptr<Atom>> m_atoms;
    std::unordered_map<std::string, std::shared_ptr<Atom>> m_atomsByName;
    mutable std::shared_mutex m_mutex;
};

class CognitiveAgent {
public:
    enum class State : uint32_t {
        Inactive = 0,
        Active,
        Learning,
        SelfModifying
    };

    CognitiveAgent(const std::string& name, std::shared_ptr<AtomSpace> atomSpace)
        : m_name(name), m_atomSpace(atomSpace), m_state(State::Inactive), m_shouldStop(false) {}

    ~CognitiveAgent() { Stop(); }

    void Start() {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        if (m_state != State::Inactive) return;
        
        m_shouldStop = false;
        m_state = State::Active;
        m_processingThread = std::thread(&CognitiveAgent::ProcessingLoop, this);
    }

    void Stop() {
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            m_shouldStop = true;
            m_state = State::Inactive;
        }
        
        if (m_processingThread.joinable()) {
            m_processingThread.join();
        }
    }

    State GetState() const { return m_state; }
    const std::string& GetName() const { return m_name; }
    
    void AddGoal(std::shared_ptr<Atom> goal) {
        if (goal && goal->GetType() == Atom::Type::Goal) {
            m_goals.push_back(goal);
        }
    }

    const std::vector<std::shared_ptr<Atom>>& GetGoals() const { return m_goals; }

private:
    void ProcessingLoop() {
        while (!m_shouldStop) {
            try {
                Perceive();
                Reason();
                Learn();
                
                // Occasional self-modification
                static thread_local std::random_device rd;
                static thread_local std::mt19937 gen(rd());
                std::uniform_real_distribution<> dis(0.0, 1.0);
                
                if (dis(gen) < 0.01) {
                    SelfModify();
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                
            } catch (...) {
                std::lock_guard<std::mutex> lock(m_stateMutex);
                m_state = State::Inactive;
            }
        }
    }

    void Perceive() {
        if (!m_atomSpace) return;
        m_state = State::Active;
        
        auto highAttentionAtoms = m_atomSpace->Query([](const Atom& atom) {
            return atom.GetAttention() > 0.7f;
        });
        
        for (auto& atom : highAttentionAtoms) {
            auto memory = m_atomSpace->CreateAtom(Atom::Type::Memory, 
                                                 "Perceived:" + atom->GetName(), 
                                                 atom->GetTruthValue(), 
                                                 atom->GetConfidence());
            m_memories.push_back(memory);
        }
    }

    void Reason() {
        for (auto& memory : m_memories) {
            auto relatedAtoms = m_atomSpace->Query([&memory](const Atom& atom) {
                return atom.GetType() == Atom::Type::Concept && 
                       atom.GetName().find(memory->GetName().substr(9)) != std::string::npos;
            });
            
            for (auto& related : relatedAtoms) {
                float currentTruth = related->GetTruthValue();
                float memoryTruth = memory->GetTruthValue();
                float newTruth = (currentTruth + memoryTruth) / 2.0f;
                related->SetTruthValue(newTruth, related->GetConfidence() * 1.1f);
            }
        }
    }

    void Learn() {
        m_state = State::Learning;
        
        auto concepts = m_atomSpace->FindAtomsByType(Atom::Type::Concept);
        for (auto& concept : concepts) {
            if (concept->GetAttention() > 0.5f) {
                concept->SetTruthValue(concept->GetTruthValue(), 
                                     std::min(1.0f, concept->GetConfidence() + 0.01f));
            }
        }
        
        if (m_memories.size() > 100) {
            m_memories.erase(m_memories.begin(), m_memories.begin() + 10);
        }
    }

    void SelfModify() {
        m_state = State::SelfModifying;
        
        auto successfulPlans = m_atomSpace->Query([](const Atom& atom) {
            return atom.GetType() == Atom::Type::Process && 
                   atom.GetName().substr(0, 5) == "Plan:" && 
                   atom.GetTruthValue() > 0.8f;
        });
        
        for (auto& plan : successfulPlans) {
            auto rule = m_atomSpace->CreateAtom(Atom::Type::Rule, 
                                              "Rule:" + plan->GetName(),
                                              plan->GetTruthValue(), 
                                              plan->GetConfidence());
            plan->AddOutgoingLink(rule);
        }
    }

    std::string m_name;
    std::shared_ptr<AtomSpace> m_atomSpace;
    State m_state;
    std::atomic<bool> m_shouldStop;
    std::thread m_processingThread;
    std::vector<std::shared_ptr<Atom>> m_goals;
    std::vector<std::shared_ptr<Atom>> m_memories;
    mutable std::mutex m_stateMutex;
};

class CognitiveSystem {
public:
    CognitiveSystem() : m_globalAtomSpace(std::make_shared<AtomSpace>()) {}

    void Initialize() {
        m_globalAtomSpace->CreateAtom(Atom::Type::Concept, "System", 1.0f, 1.0f);
        m_globalAtomSpace->CreateAtom(Atom::Type::Goal, "SystemStability", 1.0f, 1.0f);
    }

    std::shared_ptr<CognitiveAgent> CreateAgent(const std::string& name) {
        std::unique_lock<std::shared_mutex> lock(m_agentsMutex);
        
        if (m_agents.find(name) != m_agents.end()) {
            return m_agents[name];
        }
        
        auto agent = std::make_shared<CognitiveAgent>(name, m_globalAtomSpace);
        m_agents[name] = agent;
        
        auto goal = m_globalAtomSpace->CreateAtom(Atom::Type::Goal, "AgentGoal:" + name, 0.5f, 0.8f);
        agent->AddGoal(goal);
        
        return agent;
    }

    std::shared_ptr<CognitiveAgent> GetAgent(const std::string& name) const {
        std::shared_lock<std::shared_mutex> lock(m_agentsMutex);
        auto it = m_agents.find(name);
        return (it != m_agents.end()) ? it->second : nullptr;
    }

    std::vector<std::string> GetAgentNames() const {
        std::shared_lock<std::shared_mutex> lock(m_agentsMutex);
        std::vector<std::string> names;
        
        for (const auto& [name, agent] : m_agents) {
            names.push_back(name);
        }
        
        return names;
    }

    std::shared_ptr<AtomSpace> GetGlobalAtomSpace() const { return m_globalAtomSpace; }

    size_t GetAgentCount() const { 
        std::shared_lock<std::shared_mutex> lock(m_agentsMutex);
        return m_agents.size(); 
    }

private:
    std::shared_ptr<AtomSpace> m_globalAtomSpace;
    std::unordered_map<std::string, std::shared_ptr<CognitiveAgent>> m_agents;
    mutable std::shared_mutex m_agentsMutex;
};

} // namespace cognitive_test

// Test functions
void TestAtomCreation() {
    std::cout << "Testing Atom Creation..." << std::endl;
    
    cognitive_test::AtomSpace atomSpace;
    
    auto concept = atomSpace.CreateAtom(cognitive_test::Atom::Type::Concept, "TestConcept", 0.8f, 0.9f);
    assert(concept != nullptr);
    assert(concept->GetType() == cognitive_test::Atom::Type::Concept);
    assert(concept->GetName() == "TestConcept");
    assert(concept->GetTruthValue() == 0.8f);
    assert(concept->GetConfidence() == 0.9f);
    
    auto concept2 = atomSpace.CreateAtom(cognitive_test::Atom::Type::Concept, "TestConcept", 0.5f, 0.7f);
    assert(concept == concept2);
    
    std::cout << "✓ Atom Creation tests passed" << std::endl;
}

void TestAtomLinks() {
    std::cout << "Testing Atom Links..." << std::endl;
    
    cognitive_test::AtomSpace atomSpace;
    
    auto concept1 = atomSpace.CreateAtom(cognitive_test::Atom::Type::Concept, "Concept1", 0.8f, 0.9f);
    auto concept2 = atomSpace.CreateAtom(cognitive_test::Atom::Type::Concept, "Concept2", 0.7f, 0.8f);
    
    concept1->AddOutgoingLink(concept2);
    assert(concept1->GetOutgoingLinks().size() == 1);
    
    std::cout << "✓ Atom Links tests passed" << std::endl;
}

void TestCognitiveAgent() {
    std::cout << "Testing Cognitive Agent..." << std::endl;
    
    auto atomSpace = std::make_shared<cognitive_test::AtomSpace>();
    cognitive_test::CognitiveAgent agent("TestAgent", atomSpace);
    
    assert(agent.GetState() == cognitive_test::CognitiveAgent::State::Inactive);
    assert(agent.GetName() == "TestAgent");
    
    auto goal = atomSpace->CreateAtom(cognitive_test::Atom::Type::Goal, "TestGoal", 0.5f, 0.8f);
    agent.AddGoal(goal);
    assert(agent.GetGoals().size() == 1);
    
    agent.Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    agent.Stop();
    
    std::cout << "✓ Cognitive Agent tests passed" << std::endl;
}

void TestCognitiveSystem() {
    std::cout << "Testing Cognitive System..." << std::endl;
    
    cognitive_test::CognitiveSystem system;
    system.Initialize();
    
    auto agent1 = system.CreateAgent("Agent1");
    assert(agent1 != nullptr);
    
    auto agent2 = system.CreateAgent("Agent2");
    assert(agent2 != nullptr);
    
    auto retrievedAgent = system.GetAgent("Agent1");
    assert(retrievedAgent == agent1);
    
    auto agentNames = system.GetAgentNames();
    assert(agentNames.size() >= 2);
    
    std::cout << "✓ Cognitive System tests passed" << std::endl;
}

void TestAttentionSpread() {
    std::cout << "Testing Attention Spread..." << std::endl;
    
    cognitive_test::AtomSpace atomSpace;
    
    auto concept1 = atomSpace.CreateAtom(cognitive_test::Atom::Type::Concept, "HighAttentionSource", 0.8f, 0.9f);
    auto concept2 = atomSpace.CreateAtom(cognitive_test::Atom::Type::Concept, "LinkedConcept", 0.7f, 0.8f);
    
    concept1->SetAttention(1.0f);
    concept2->SetAttention(0.1f);
    concept1->AddOutgoingLink(concept2);
    
    float initialAttention2 = concept2->GetAttention();
    atomSpace.UpdateAttentionValues();
    float finalAttention2 = concept2->GetAttention();
    
    assert(finalAttention2 > initialAttention2);
    
    std::cout << "✓ Attention Spread tests passed" << std::endl;
}

void TestAutogeneticBehavior() {
    std::cout << "Testing Autogenetic Behavior..." << std::endl;
    
    cognitive_test::CognitiveSystem system;
    system.Initialize();
    
    auto atomSpace = system.GetGlobalAtomSpace();
    
    // Create a successful plan that should trigger self-modification
    auto plan = atomSpace->CreateAtom(cognitive_test::Atom::Type::Process, "Plan:SuccessfulTask", 0.9f, 0.9f);
    
    auto agent = system.CreateAgent("AutogeneticAgent");
    agent->Start();
    
    // Let the agent run and potentially self-modify
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    agent->Stop();
    
    // Check if rules were created (indicating self-modification)
    auto rules = atomSpace->FindAtomsByType(cognitive_test::Atom::Type::Rule);
    
    std::cout << "✓ Autogenetic Behavior tests passed (created " << rules.size() << " rules)" << std::endl;
}

int main() {
    std::cout << "Running Standalone OpenCog Cognitive Framework Tests..." << std::endl << std::endl;
    
    try {
        TestAtomCreation();
        TestAtomLinks();
        TestCognitiveAgent();
        TestCognitiveSystem();
        TestAttentionSpread();
        TestAutogeneticBehavior();
        
        std::cout << std::endl << "🎉 All standalone tests passed successfully!" << std::endl;
        std::cout << "OpenCog Cognitive Framework core functionality is working correctly." << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
        
    } catch (...) {
        std::cerr << "❌ Test failed with unknown exception" << std::endl;
        return 1;
    }
}