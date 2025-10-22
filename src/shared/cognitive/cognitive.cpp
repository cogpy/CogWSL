/*++

Copyright (c) Microsoft. All rights reserved.

Module Name:

    cognitive.cpp

Abstract:

    Implementation of the OpenCog cognitive architecture framework 
    for autogenetic WSL capabilities.

--*/

#include <shared_mutex>
#include <thread>
#include <algorithm>
#include <random>
#include "../inc/cognitive.h"

namespace wsl::shared::cognitive {

// Static member initialization
std::atomic<uint64_t> Atom::s_nextId{1};
std::unique_ptr<CognitiveSystem> g_cognitiveSystem = nullptr;

// Atom Implementation
Atom::Atom(Type type, const std::string& name, float truthValue, float confidence)
    : m_type(type)
    , m_name(name)
    , m_id(s_nextId++)
    , m_truthValue(truthValue)
    , m_confidence(confidence)
    , m_attention(0.5f)
    , m_creationTime(std::chrono::system_clock::now())
    , m_lastAccessed(m_creationTime)
{
}

void Atom::SetTruthValue(float truth, float confidence) {
    m_truthValue = std::clamp(truth, 0.0f, 1.0f);
    m_confidence = std::clamp(confidence, 0.0f, 1.0f);
    m_lastAccessed = std::chrono::system_clock::now();
}

void Atom::AddIncomingLink(std::shared_ptr<Atom> atom) {
    if (atom && std::find(m_incomingLinks.begin(), m_incomingLinks.end(), atom) == m_incomingLinks.end()) {
        m_incomingLinks.push_back(atom);
        m_lastAccessed = std::chrono::system_clock::now();
    }
}

void Atom::AddOutgoingLink(std::shared_ptr<Atom> atom) {
    if (atom && std::find(m_outgoingLinks.begin(), m_outgoingLinks.end(), atom) == m_outgoingLinks.end()) {
        m_outgoingLinks.push_back(atom);
        m_lastAccessed = std::chrono::system_clock::now();
    }
}

// AtomSpace Implementation
AtomSpace::AtomSpace() {
    // Create fundamental system atoms
    CreateAtom(Atom::Type::Concept, "Self", 1.0f, 1.0f);
    CreateAtom(Atom::Type::Concept, "System", 1.0f, 1.0f);
    CreateAtom(Atom::Type::Concept, "WSL", 1.0f, 1.0f);
}

AtomSpace::~AtomSpace() {
    Clear();
}

std::shared_ptr<Atom> AtomSpace::CreateAtom(Atom::Type type, const std::string& name, 
                                          float truthValue, float confidence) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    
    // Check if atom with this name already exists
    auto existing = m_atomsByName.find(name);
    if (existing != m_atomsByName.end()) {
        return existing->second;
    }
    
    auto atom = std::make_shared<Atom>(type, name, truthValue, confidence);
    m_atoms[atom->GetId()] = atom;
    m_atomsByName[name] = atom;
    
    return atom;
}

std::shared_ptr<Atom> AtomSpace::GetAtom(uint64_t id) const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    auto it = m_atoms.find(id);
    return (it != m_atoms.end()) ? it->second : nullptr;
}

std::shared_ptr<Atom> AtomSpace::FindAtom(const std::string& name) const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    auto it = m_atomsByName.find(name);
    return (it != m_atomsByName.end()) ? it->second : nullptr;
}

std::vector<std::shared_ptr<Atom>> AtomSpace::FindAtomsByType(Atom::Type type) const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    std::vector<std::shared_ptr<Atom>> result;
    
    for (const auto& [id, atom] : m_atoms) {
        if (atom->GetType() == type) {
            result.push_back(atom);
        }
    }
    
    return result;
}

bool AtomSpace::RemoveAtom(uint64_t id) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    
    auto it = m_atoms.find(id);
    if (it == m_atoms.end()) {
        return false;
    }
    
    auto atom = it->second;
    m_atomsByName.erase(atom->GetName());
    m_atoms.erase(it);
    
    return true;
}

void AtomSpace::Clear() {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_atoms.clear();
    m_atomsByName.clear();
}

std::vector<std::shared_ptr<Atom>> AtomSpace::Query(const std::function<bool(const Atom&)>& predicate) const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    std::vector<std::shared_ptr<Atom>> result;
    
    for (const auto& [id, atom] : m_atoms) {
        if (predicate(*atom)) {
            result.push_back(atom);
        }
    }
    
    return result;
}

void AtomSpace::UpdateAttentionValues() {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    
    // Simple attention decay and spreading
    for (const auto& [id, atom] : m_atoms) {
        float currentAttention = atom->GetAttention();
        
        // Decay attention over time
        float newAttention = currentAttention * 0.95f;
        
        // Spread attention to linked atoms
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

size_t AtomSpace::GetAtomCountByType(Atom::Type type) const {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    size_t count = 0;
    
    for (const auto& [id, atom] : m_atoms) {
        if (atom->GetType() == type) {
            count++;
        }
    }
    
    return count;
}

// CognitiveAgent Implementation
CognitiveAgent::CognitiveAgent(const std::string& name, std::shared_ptr<AtomSpace> atomSpace)
    : m_name(name)
    , m_atomSpace(atomSpace)
    , m_state(State::Inactive)
    , m_shouldStop(false)
{
    // Create agent's self-concept in the atomspace
    if (m_atomSpace) {
        auto selfConcept = m_atomSpace->CreateAtom(Atom::Type::Agent, "Agent:" + name, 1.0f, 1.0f);
        selfConcept->SetAttention(1.0f);
    }
}

CognitiveAgent::~CognitiveAgent() {
    Stop();
}

void CognitiveAgent::Start() {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    
    if (m_state != State::Inactive) {
        return;
    }
    
    m_shouldStop = false;
    m_state = State::Active;
    m_processingThread = std::thread(&CognitiveAgent::ProcessingLoop, this);
}

void CognitiveAgent::Stop() {
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_shouldStop = true;
        m_state = State::Inactive;
    }
    
    m_stateCondition.notify_all();
    
    if (m_processingThread.joinable()) {
        m_processingThread.join();
    }
}

void CognitiveAgent::Pause() {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    if (m_state == State::Active) {
        m_state = State::Inactive;
    }
}

void CognitiveAgent::Resume() {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    if (m_state == State::Inactive && !m_shouldStop) {
        m_state = State::Active;
        m_stateCondition.notify_all();
    }
}

void CognitiveAgent::ProcessingLoop() {
    while (!m_shouldStop) {
        try {
            // Main cognitive cycle
            Perceive();
            Reason();
            Plan();
            Act();
            Learn();
            
            // Occasional self-modification
            static thread_local std::random_device rd;
            static thread_local std::mt19937 gen(rd());
            std::uniform_real_distribution<> dis(0.0, 1.0);
            
            if (dis(gen) < 0.01) { // 1% chance per cycle
                SelfModify();
            }
            
            // Sleep to prevent excessive CPU usage
            std::unique_lock<std::mutex> lock(m_stateMutex);
            m_stateCondition.wait_for(lock, std::chrono::milliseconds(100), 
                                    [this] { return m_shouldStop || m_state != State::Active; });
            
        } catch (...) {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            m_state = State::Error;
        }
    }
}

void CognitiveAgent::Perceive() {
    if (!m_atomSpace) return;
    
    m_state = State::Active;
    
    // Simple perception: gather high-attention atoms
    auto highAttentionAtoms = m_atomSpace->Query([](const Atom& atom) {
        return atom.GetAttention() > 0.7f;
    });
    
    // Process perceived information
    for (auto& atom : highAttentionAtoms) {
        // Create memory of perception
        auto memory = m_atomSpace->CreateAtom(Atom::Type::Memory, 
                                             "Perceived:" + atom->GetName(), 
                                             atom->GetTruthValue(), 
                                             atom->GetConfidence());
        m_memories.push_back(memory);
    }
}

void CognitiveAgent::Reason() {
    if (!m_atomSpace) return;
    
    // Simple reasoning: update beliefs based on evidence
    for (auto& memory : m_memories) {
        // Find related concepts
        auto relatedAtoms = m_atomSpace->Query([&memory](const Atom& atom) {
            return atom.GetType() == Atom::Type::Concept && 
                   atom.GetName().find(memory->GetName().substr(9)) != std::string::npos; // Remove "Perceived:" prefix
        });
        
        // Update truth values based on evidence
        for (auto& related : relatedAtoms) {
            float currentTruth = related->GetTruthValue();
            float memoryTruth = memory->GetTruthValue();
            float newTruth = (currentTruth + memoryTruth) / 2.0f;
            related->SetTruthValue(newTruth, related->GetConfidence() * 1.1f);
        }
    }
}

void CognitiveAgent::Plan() {
    // Simple planning based on goals
    for (auto& goal : m_goals) {
        if (goal->GetTruthValue() < 0.8f) { // Goal not achieved
            // Create action plan
            auto plan = m_atomSpace->CreateAtom(Atom::Type::Process, 
                                              "Plan:" + goal->GetName(),
                                              0.5f, 0.8f);
            goal->AddOutgoingLink(plan);
        }
    }
}

void CognitiveAgent::Act() {
    // Execute plans
    auto plans = m_atomSpace->FindAtomsByType(Atom::Type::Process);
    for (auto& plan : plans) {
        if (plan->GetName().substr(0, 5) == "Plan:" && plan->GetTruthValue() > 0.4f) {
            // Simulate action execution
            plan->SetTruthValue(plan->GetTruthValue() + 0.1f, plan->GetConfidence());
            
            // Update associated goal
            for (auto& incoming : plan->GetIncomingLinks()) {
                if (incoming->GetType() == Atom::Type::Goal) {
                    incoming->SetTruthValue(incoming->GetTruthValue() + 0.05f, incoming->GetConfidence());
                }
            }
        }
    }
}

void CognitiveAgent::Learn() {
    if (!m_atomSpace) return;
    
    m_state = State::Learning;
    
    // Simple learning: strengthen frequently accessed patterns
    auto concepts = m_atomSpace->FindAtomsByType(Atom::Type::Concept);
    for (auto& concept : concepts) {
        if (concept->GetAttention() > 0.5f) {
            concept->SetTruthValue(concept->GetTruthValue(), 
                                 std::min(1.0f, concept->GetConfidence() + 0.01f));
        }
    }
    
    // Clean up old memories to prevent unbounded growth
    if (m_memories.size() > 1000) {
        m_memories.erase(m_memories.begin(), m_memories.begin() + 100);
    }
}

void CognitiveAgent::SelfModify() {
    if (!m_atomSpace) return;
    
    m_state = State::SelfModifying;
    
    // Simple self-modification: create new rules based on successful patterns
    auto successfulPlans = m_atomSpace->Query([](const Atom& atom) {
        return atom.GetType() == Atom::Type::Process && 
               atom.GetName().substr(0, 5) == "Plan:" && 
               atom.GetTruthValue() > 0.8f;
    });
    
    for (auto& plan : successfulPlans) {
        // Create a rule based on successful plan
        auto rule = m_atomSpace->CreateAtom(Atom::Type::Rule, 
                                          "Rule:" + plan->GetName(),
                                          plan->GetTruthValue(), 
                                          plan->GetConfidence());
        plan->AddOutgoingLink(rule);
    }
}

void CognitiveAgent::AddGoal(std::shared_ptr<Atom> goal) {
    if (goal && goal->GetType() == Atom::Type::Goal) {
        m_goals.push_back(goal);
    }
}

void CognitiveAgent::RemoveGoal(uint64_t goalId) {
    m_goals.erase(std::remove_if(m_goals.begin(), m_goals.end(),
                                [goalId](const std::shared_ptr<Atom>& goal) {
                                    return goal->GetId() == goalId;
                                }), m_goals.end());
}

void CognitiveAgent::SendMessage(const std::string& targetAgent, const std::string& message) {
    // TODO: Implement inter-agent communication
    // This would integrate with WSL's message passing system
}

void CognitiveAgent::ReceiveMessage(const std::string& fromAgent, const std::string& message) {
    // Create memory of received message
    if (m_atomSpace) {
        auto messageAtom = m_atomSpace->CreateAtom(Atom::Type::Memory, 
                                                 "Message:" + fromAgent + ":" + message,
                                                 1.0f, 0.9f);
        m_memories.push_back(messageAtom);
    }
}

bool CognitiveAgent::ShouldContinueProcessing() const {
    return !m_shouldStop && m_state == State::Active;
}

// CognitiveSystem Implementation
CognitiveSystem::CognitiveSystem() 
    : m_globalAtomSpace(std::make_shared<AtomSpace>())
    , m_startTime(std::chrono::system_clock::now())
    , m_initialized(false)
{
}

CognitiveSystem::~CognitiveSystem() {
    Shutdown();
}

void CognitiveSystem::Initialize() {
    if (m_initialized.exchange(true)) {
        return; // Already initialized
    }
    
    // Create system-level concepts
    m_globalAtomSpace->CreateAtom(Atom::Type::Concept, "CognitiveSystem", 1.0f, 1.0f);
    m_globalAtomSpace->CreateAtom(Atom::Type::Goal, "SystemStability", 1.0f, 1.0f);
    m_globalAtomSpace->CreateAtom(Atom::Type::Goal, "OptimizePerformance", 0.8f, 0.9f);
    
    // Set default configuration
    SetConfiguration("max_agents", "10");
    SetConfiguration("attention_update_interval", "1000");
    SetConfiguration("self_modification_probability", "0.01");
}

void CognitiveSystem::Shutdown() {
    if (!m_initialized.exchange(false)) {
        return; // Already shutdown
    }
    
    // Stop all agents
    std::unique_lock<std::shared_mutex> lock(m_agentsMutex);
    for (auto& [name, agent] : m_agents) {
        agent->Stop();
    }
    m_agents.clear();
}

std::shared_ptr<CognitiveAgent> CognitiveSystem::CreateAgent(const std::string& name) {
    std::unique_lock<std::shared_mutex> lock(m_agentsMutex);
    
    if (m_agents.find(name) != m_agents.end()) {
        return m_agents[name]; // Agent already exists
    }
    
    auto agent = std::make_shared<CognitiveAgent>(name, m_globalAtomSpace);
    m_agents[name] = agent;
    
    // Create agent goal
    auto goal = m_globalAtomSpace->CreateAtom(Atom::Type::Goal, "AgentGoal:" + name, 0.5f, 0.8f);
    agent->AddGoal(goal);
    
    return agent;
}

std::shared_ptr<CognitiveAgent> CognitiveSystem::GetAgent(const std::string& name) const {
    std::shared_lock<std::shared_mutex> lock(m_agentsMutex);
    auto it = m_agents.find(name);
    return (it != m_agents.end()) ? it->second : nullptr;
}

bool CognitiveSystem::RemoveAgent(const std::string& name) {
    std::unique_lock<std::shared_mutex> lock(m_agentsMutex);
    
    auto it = m_agents.find(name);
    if (it == m_agents.end()) {
        return false;
    }
    
    it->second->Stop();
    m_agents.erase(it);
    return true;
}

std::vector<std::string> CognitiveSystem::GetAgentNames() const {
    std::shared_lock<std::shared_mutex> lock(m_agentsMutex);
    std::vector<std::string> names;
    
    for (const auto& [name, agent] : m_agents) {
        names.push_back(name);
    }
    
    return names;
}

void CognitiveSystem::BroadcastMessage(const std::string& message) {
    std::shared_lock<std::shared_mutex> lock(m_agentsMutex);
    
    for (const auto& [name, agent] : m_agents) {
        agent->ReceiveMessage("System", message);
    }
}

void CognitiveSystem::UpdateSystem() {
    if (!m_initialized) {
        return;
    }
    
    // Update global attention values
    m_globalAtomSpace->UpdateAttentionValues();
    
    // System-level reasoning and self-optimization
    auto systemGoals = m_globalAtomSpace->FindAtomsByType(Atom::Type::Goal);
    for (auto& goal : systemGoals) {
        if (goal->GetName() == "OptimizePerformance") {
            // Adjust system parameters based on performance
            auto stats = GetStatistics();
            if (stats.activeAgents < stats.totalAgents / 2) {
                // Activate more agents
                std::shared_lock<std::shared_mutex> agentLock(m_agentsMutex);
                for (auto& [name, agent] : m_agents) {
                    if (agent->GetState() == CognitiveAgent::State::Inactive) {
                        agent->Resume();
                        break;
                    }
                }
            }
        }
    }
}

void CognitiveSystem::SetConfiguration(const std::string& key, const std::string& value) {
    std::unique_lock<std::shared_mutex> lock(m_configMutex);
    m_configuration[key] = value;
}

std::string CognitiveSystem::GetConfiguration(const std::string& key) const {
    std::shared_lock<std::shared_mutex> lock(m_configMutex);
    auto it = m_configuration.find(key);
    return (it != m_configuration.end()) ? it->second : "";
}

CognitiveSystem::SystemStats CognitiveSystem::GetStatistics() const {
    SystemStats stats{};
    
    {
        std::shared_lock<std::shared_mutex> lock(m_agentsMutex);
        stats.totalAgents = m_agents.size();
        
        for (const auto& [name, agent] : m_agents) {
            if (agent->GetState() == CognitiveAgent::State::Active ||
                agent->GetState() == CognitiveAgent::State::Learning ||
                agent->GetState() == CognitiveAgent::State::Planning ||
                agent->GetState() == CognitiveAgent::State::Executing ||
                agent->GetState() == CognitiveAgent::State::SelfModifying) {
                stats.activeAgents++;
            }
        }
    }
    
    stats.totalAtoms = m_globalAtomSpace->GetAtomCount();
    
    // Calculate average attention
    auto allAtoms = m_globalAtomSpace->Query([](const Atom&) { return true; });
    if (!allAtoms.empty()) {
        float totalAttention = 0.0f;
        for (const auto& atom : allAtoms) {
            totalAttention += atom->GetAttention();
        }
        stats.averageAttention = totalAttention / allAtoms.size();
    }
    
    stats.uptime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now() - m_startTime);
    
    return stats;
}

} // namespace wsl::shared::cognitive