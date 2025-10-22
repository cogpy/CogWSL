/*++

Copyright (c) Microsoft. All rights reserved.

Module Name:

    wsl_cognitive_integration.cpp

Abstract:

    Implementation of WSL-OpenCog cognitive architecture integration.
    Provides autogenetic capabilities within the WSL framework.

--*/

#include "../inc/wsl_cognitive_integration.h"
#include <chrono>
#include <sstream>
#include <algorithm>
#include <random>

namespace wsl::shared::integration {

// Global integration manager instance
std::unique_ptr<CognitiveIntegrationManager> g_cognitiveIntegration = nullptr;

// CognitiveIntegrationManager Implementation
CognitiveIntegrationManager::CognitiveIntegrationManager()
    : m_cognitiveSystem(nullptr)
    , m_initialized(false)
    , m_startTime(std::chrono::system_clock::now())
{
    // Initialize statistics
    m_stats = {};
}

CognitiveIntegrationManager::~CognitiveIntegrationManager() {
    Shutdown();
}

bool CognitiveIntegrationManager::Initialize() {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    
    if (m_initialized.exchange(true)) {
        return true; // Already initialized
    }
    
    try {
        // Initialize cognitive system
        m_cognitiveSystem = std::make_unique<cognitive::CognitiveSystem>();
        m_cognitiveSystem->Initialize();
        
        // Create fundamental WSL-related concepts
        auto atomSpace = m_cognitiveSystem->GetGlobalAtomSpace();
        atomSpace->CreateAtom(cognitive::Atom::Type::Concept, "WSL_Process", 1.0f, 1.0f);
        atomSpace->CreateAtom(cognitive::Atom::Type::Concept, "WSL_Distribution", 1.0f, 1.0f);
        atomSpace->CreateAtom(cognitive::Atom::Type::Concept, "WSL_System", 1.0f, 1.0f);
        atomSpace->CreateAtom(cognitive::Atom::Type::Goal, "OptimizeWSLPerformance", 0.8f, 0.9f);
        atomSpace->CreateAtom(cognitive::Atom::Type::Goal, "EnsureSystemSecurity", 1.0f, 1.0f);
        
        // Create default cognitive agents
        CreateCognitiveAgent("SystemMonitor", "monitoring");
        CreateCognitiveAgent("ProcessOptimizer", "optimization");
        CreateCognitiveAgent("SecurityAnalyzer", "security");
        
        return true;
        
    } catch (...) {
        m_initialized = false;
        return false;
    }
}

void CognitiveIntegrationManager::Shutdown() {
    if (!m_initialized.exchange(false)) {
        return; // Already shutdown
    }
    
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    
    if (m_cognitiveSystem) {
        m_cognitiveSystem->Shutdown();
        m_cognitiveSystem.reset();
    }
    
    m_eventCallbacks.clear();
    m_processMapping.clear();
}

void CognitiveIntegrationManager::OnProcessCreate(const std::string& distroId, uint32_t processId, const std::string& command) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    
    if (!m_cognitiveSystem) return;
    
    // Create cognitive representation of the process
    auto atomSpace = m_cognitiveSystem->GetGlobalAtomSpace();
    std::string processName = "Process:" + distroId + ":" + std::to_string(processId);
    auto processAtom = atomSpace->CreateAtom(cognitive::Atom::Type::Process, processName, 1.0f, 0.8f);
    
    // Link to command concept
    std::string commandName = "Command:" + command;
    auto commandAtom = atomSpace->CreateAtom(cognitive::Atom::Type::Concept, commandName, 0.7f, 0.6f);
    processAtom->AddOutgoingLink(commandAtom);
    
    // Track the process
    m_processMapping[processName] = processId;
    
    // Notify cognitive agents
    ProcessWslEvent("process_create", distroId, processName + ":" + command);
    
    // Update statistics
    std::lock_guard<std::mutex> statsLock(m_statsMutex);
    m_stats.processesMonitored++;
    m_stats.eventsHandled++;
}

void CognitiveIntegrationManager::OnProcessDestroy(const std::string& distroId, uint32_t processId, int exitCode) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    
    if (!m_cognitiveSystem) return;
    
    std::string processName = "Process:" + distroId + ":" + std::to_string(processId);
    
    // Update process knowledge with exit information
    auto atomSpace = m_cognitiveSystem->GetGlobalAtomSpace();
    auto processAtom = atomSpace->FindAtom(processName);
    if (processAtom) {
        // Adjust truth value based on exit code
        float truthValue = (exitCode == 0) ? 1.0f : 0.3f;
        processAtom->SetTruthValue(truthValue, processAtom->GetConfidence() + 0.1f);
        
        // Create memory of the process completion
        std::string memoryName = "Completion:" + processName + ":" + std::to_string(exitCode);
        auto memoryAtom = atomSpace->CreateAtom(cognitive::Atom::Type::Memory, memoryName, truthValue, 0.9f);
        processAtom->AddOutgoingLink(memoryAtom);
    }
    
    // Remove from tracking
    m_processMapping.erase(processName);
    
    // Notify cognitive agents
    ProcessWslEvent("process_destroy", distroId, processName + ":" + std::to_string(exitCode));
    
    // Update statistics
    std::lock_guard<std::mutex> statsLock(m_statsMutex);
    m_stats.eventsHandled++;
}

void CognitiveIntegrationManager::OnDistroEvent(const std::string& distroId, const std::string& eventType, const std::string& data) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    
    if (!m_cognitiveSystem) return;
    
    // Update cognitive knowledge about the distribution
    UpdateCognitiveKnowledge("Distro:" + distroId, eventType + ":" + data, 0.8f);
    
    // Notify cognitive agents
    ProcessWslEvent("distro_event", distroId, eventType + ":" + data);
    
    // Update statistics
    std::lock_guard<std::mutex> statsLock(m_statsMutex);
    m_stats.eventsHandled++;
}

void CognitiveIntegrationManager::OnSystemEvent(const std::string& eventType, const std::string& data) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    
    if (!m_cognitiveSystem) return;
    
    // Update system-level cognitive knowledge
    UpdateCognitiveKnowledge("System", eventType + ":" + data, 0.9f);
    
    // Notify cognitive agents
    ProcessWslEvent("system_event", "system", eventType + ":" + data);
    
    // Update statistics
    std::lock_guard<std::mutex> statsLock(m_statsMutex);
    m_stats.eventsHandled++;
}

std::shared_ptr<cognitive::CognitiveAgent> CognitiveIntegrationManager::CreateCognitiveAgent(const std::string& name, const std::string& role) {
    if (!m_cognitiveSystem) return nullptr;
    
    auto agent = m_cognitiveSystem->CreateAgent(name);
    if (!agent) return nullptr;
    
    // Add role-specific goals and knowledge
    auto atomSpace = m_cognitiveSystem->GetGlobalAtomSpace();
    
    if (role == "monitoring") {
        auto goal = atomSpace->CreateAtom(cognitive::Atom::Type::Goal, "MonitorSystem:" + name, 0.9f, 0.8f);
        agent->AddGoal(goal);
    } else if (role == "optimization") {
        auto goal = atomSpace->CreateAtom(cognitive::Atom::Type::Goal, "OptimizePerformance:" + name, 0.8f, 0.9f);
        agent->AddGoal(goal);
    } else if (role == "security") {
        auto goal = atomSpace->CreateAtom(cognitive::Atom::Type::Goal, "EnsureSecurity:" + name, 1.0f, 1.0f);
        agent->AddGoal(goal);
    }
    
    // Start the agent
    agent->Start();
    
    // Update statistics
    std::lock_guard<std::mutex> statsLock(m_statsMutex);
    m_stats.cognitiveAgents++;
    
    return agent;
}

bool CognitiveIntegrationManager::DestroyCognitiveAgent(const std::string& name) {
    if (!m_cognitiveSystem) return false;
    
    bool result = m_cognitiveSystem->RemoveAgent(name);
    
    if (result) {
        std::lock_guard<std::mutex> statsLock(m_statsMutex);
        if (m_stats.cognitiveAgents > 0) {
            m_stats.cognitiveAgents--;
        }
    }
    
    return result;
}

std::string CognitiveIntegrationManager::QueryCognitiveState(const std::string& query) {
    auto startTime = std::chrono::steady_clock::now();
    
    if (!m_cognitiveSystem) {
        return "Error: Cognitive system not initialized";
    }
    
    std::string response = GenerateCognitiveResponse(query);
    
    // Update response time statistics
    auto endTime = std::chrono::steady_clock::now();
    auto responseTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    std::lock_guard<std::mutex> statsLock(m_statsMutex);
    m_stats.activeQueries++;
    m_stats.averageResponseTime = 
        (m_stats.averageResponseTime + responseTime) / std::chrono::milliseconds(2);
    
    return response;
}

bool CognitiveIntegrationManager::ExecuteCognitiveCommand(const std::string& agentName, const std::string& command, const std::string& parameters) {
    if (!m_cognitiveSystem) return false;
    
    auto agent = m_cognitiveSystem->GetAgent(agentName);
    if (!agent) return false;
    
    // Send command as a message to the agent
    agent->ReceiveMessage("System", command + ":" + parameters);
    
    return true;
}

void CognitiveIntegrationManager::SetCognitiveConfiguration(const std::string& key, const std::string& value) {
    if (m_cognitiveSystem) {
        m_cognitiveSystem->SetConfiguration(key, value);
    }
}

std::string CognitiveIntegrationManager::GetCognitiveConfiguration(const std::string& key) const {
    if (m_cognitiveSystem) {
        return m_cognitiveSystem->GetConfiguration(key);
    }
    return "";
}

void CognitiveIntegrationManager::RegisterEventCallback(const std::string& eventType, EventCallback callback) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_eventCallbacks[eventType] = callback;
}

void CognitiveIntegrationManager::UnregisterEventCallback(const std::string& eventType) {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_eventCallbacks.erase(eventType);
}

CognitiveIntegrationManager::IntegrationStats CognitiveIntegrationManager::GetStatistics() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_stats;
}

void CognitiveIntegrationManager::ProcessWslEvent(const std::string& eventType, const std::string& source, const std::string& data) {
    // Call registered callbacks
    auto it = m_eventCallbacks.find(eventType);
    if (it != m_eventCallbacks.end()) {
        try {
            it->second(source, data);
        } catch (...) {
            // Log error but continue processing
        }
    }
    
    // Broadcast to all cognitive agents
    if (m_cognitiveSystem) {
        m_cognitiveSystem->BroadcastMessage(eventType + ":" + source + ":" + data);
    }
}

void CognitiveIntegrationManager::UpdateCognitiveKnowledge(const std::string& concept, const std::string& information, float confidence) {
    if (!m_cognitiveSystem) return;
    
    auto atomSpace = m_cognitiveSystem->GetGlobalAtomSpace();
    
    // Create or update concept
    auto conceptAtom = atomSpace->CreateAtom(cognitive::Atom::Type::Concept, concept, 0.5f, confidence);
    
    // Create information atom
    auto infoAtom = atomSpace->CreateAtom(cognitive::Atom::Type::Memory, 
                                         concept + "_Info:" + information, 
                                         0.8f, confidence);
    
    // Link concept to information
    conceptAtom->AddOutgoingLink(infoAtom);
    
    // Update attention
    conceptAtom->SetAttention(conceptAtom->GetAttention() + 0.1f);
}

std::string CognitiveIntegrationManager::GenerateCognitiveResponse(const std::string& query) {
    if (!m_cognitiveSystem) return "System not available";
    
    auto atomSpace = m_cognitiveSystem->GetGlobalAtomSpace();
    std::ostringstream response;
    
    // Parse query and generate response based on cognitive state
    if (query.find("status") != std::string::npos) {
        auto stats = m_cognitiveSystem->GetStatistics();
        response << "Cognitive System Status:\n"
                << "- Total Agents: " << stats.totalAgents << "\n"
                << "- Active Agents: " << stats.activeAgents << "\n"
                << "- Total Atoms: " << stats.totalAtoms << "\n"
                << "- Average Attention: " << stats.averageAttention << "\n"
                << "- Uptime: " << stats.uptime.count() << "ms";
                
    } else if (query.find("processes") != std::string::npos) {
        auto processAtoms = atomSpace->FindAtomsByType(cognitive::Atom::Type::Process);
        response << "Monitored Processes (" << processAtoms.size() << "):\n";
        for (const auto& atom : processAtoms) {
            response << "- " << atom->GetName() 
                    << " (Truth: " << atom->GetTruthValue() 
                    << ", Attention: " << atom->GetAttention() << ")\n";
        }
        
    } else if (query.find("agents") != std::string::npos) {
        auto agentNames = m_cognitiveSystem->GetAgentNames();
        response << "Cognitive Agents (" << agentNames.size() << "):\n";
        for (const auto& name : agentNames) {
            auto agent = m_cognitiveSystem->GetAgent(name);
            if (agent) {
                response << "- " << name << " (State: ";
                switch (agent->GetState()) {
                    case cognitive::CognitiveAgent::State::Active: response << "Active"; break;
                    case cognitive::CognitiveAgent::State::Learning: response << "Learning"; break;
                    case cognitive::CognitiveAgent::State::Planning: response << "Planning"; break;
                    case cognitive::CognitiveAgent::State::Executing: response << "Executing"; break;
                    case cognitive::CognitiveAgent::State::SelfModifying: response << "Self-Modifying"; break;
                    case cognitive::CognitiveAgent::State::Inactive: response << "Inactive"; break;
                    case cognitive::CognitiveAgent::State::Error: response << "Error"; break;
                }
                response << ")\n";
            }
        }
        
    } else {
        // General query - search for relevant concepts
        auto allAtoms = atomSpace->Query([&query](const cognitive::Atom& atom) {
            return atom.GetName().find(query) != std::string::npos && atom.GetAttention() > 0.3f;
        });
        
        response << "Query Results for '" << query << "':\n";
        for (const auto& atom : allAtoms) {
            response << "- " << atom->GetName() 
                    << " (Type: ";
            switch (atom.GetType()) {
                case cognitive::Atom::Type::Concept: response << "Concept"; break;
                case cognitive::Atom::Type::Process: response << "Process"; break;
                case cognitive::Atom::Type::Agent: response << "Agent"; break;
                case cognitive::Atom::Type::Goal: response << "Goal"; break;
                case cognitive::Atom::Type::Memory: response << "Memory"; break;
                case cognitive::Atom::Type::Rule: response << "Rule"; break;
                case cognitive::Atom::Type::Link: response << "Link"; break;
            }
            response << ", Truth: " << atom.GetTruthValue() << ")\n";
        }
    }
    
    return response.str();
}

void CognitiveIntegrationManager::HandleSelfModificationEvent(const std::string& agentName, const std::string& modification) {
    // Log and analyze self-modification events
    UpdateCognitiveKnowledge("SelfModification:" + agentName, modification, 0.9f);
    
    // Broadcast to other agents for learning
    if (m_cognitiveSystem) {
        m_cognitiveSystem->BroadcastMessage("self_modification:" + agentName + ":" + modification);
    }
}

// CognitiveProcessMonitor Implementation
CognitiveProcessMonitor::CognitiveProcessMonitor(std::shared_ptr<CognitiveIntegrationManager> integrationManager)
    : m_integrationManager(integrationManager)
    , m_monitoring(false)
{
}

CognitiveProcessMonitor::~CognitiveProcessMonitor() {
    StopMonitoring();
}

void CognitiveProcessMonitor::StartMonitoring() {
    if (m_monitoring.exchange(true)) {
        return; // Already monitoring
    }
    
    m_monitoringThread = std::thread(&CognitiveProcessMonitor::MonitoringLoop, this);
}

void CognitiveProcessMonitor::StopMonitoring() {
    if (!m_monitoring.exchange(false)) {
        return; // Already stopped
    }
    
    if (m_monitoringThread.joinable()) {
        m_monitoringThread.join();
    }
}

void CognitiveProcessMonitor::TrackProcess(const std::string& distroId, uint32_t processId, const std::string& command) {
    std::unique_lock<std::shared_mutex> lock(m_processMutex);
    
    std::string processKey = distroId + ":" + std::to_string(processId);
    
    ProcessInfo info;
    info.distroId = distroId;
    info.processId = processId;
    info.command = command;
    info.startTime = std::chrono::system_clock::now();
    
    // Create cognitive representation
    if (m_integrationManager && m_integrationManager->GetCognitiveSystem()) {
        auto atomSpace = m_integrationManager->GetCognitiveSystem()->GetGlobalAtomSpace();
        info.cognitiveRepresentation = atomSpace->CreateAtom(
            cognitive::Atom::Type::Process, 
            "MonitoredProcess:" + processKey,
            1.0f, 0.8f
        );
    }
    
    m_trackedProcesses[processKey] = std::move(info);
}

void CognitiveProcessMonitor::UntrackProcess(const std::string& distroId, uint32_t processId) {
    std::unique_lock<std::shared_mutex> lock(m_processMutex);
    
    std::string processKey = distroId + ":" + std::to_string(processId);
    
    auto it = m_trackedProcesses.find(processKey);
    if (it != m_trackedProcesses.end()) {
        // Final knowledge update
        UpdateProcessKnowledge(it->second);
        m_trackedProcesses.erase(it);
    }
}

void CognitiveProcessMonitor::AnalyzeProcessBehavior(const std::string& distroId, uint32_t processId) {
    std::shared_lock<std::shared_mutex> lock(m_processMutex);
    
    std::string processKey = distroId + ":" + std::to_string(processId);
    
    auto it = m_trackedProcesses.find(processKey);
    if (it != m_trackedProcesses.end()) {
        UpdateProcessKnowledge(it->second);
    }
}

void CognitiveProcessMonitor::LearnFromProcessPatterns() {
    std::shared_lock<std::shared_mutex> lock(m_processMutex);
    
    // Analyze patterns across all tracked processes
    std::unordered_map<std::string, int> commandCounts;
    std::unordered_map<std::string, std::chrono::milliseconds> averageDurations;
    
    for (const auto& [key, process] : m_trackedProcesses) {
        commandCounts[process.command]++;
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() - process.startTime);
        averageDurations[process.command] = 
            (averageDurations[process.command] + duration) / std::chrono::milliseconds(2);
    }
    
    // Update cognitive knowledge with learned patterns
    if (m_integrationManager && m_integrationManager->GetCognitiveSystem()) {
        auto atomSpace = m_integrationManager->GetCognitiveSystem()->GetGlobalAtomSpace();
        
        for (const auto& [command, count] : commandCounts) {
            auto patternAtom = atomSpace->CreateAtom(
                cognitive::Atom::Type::Rule,
                "Pattern:" + command + "_frequency",
                static_cast<float>(count) / 100.0f, // Normalize to 0-1 range
                0.8f
            );
            
            auto durationAtom = atomSpace->CreateAtom(
                cognitive::Atom::Type::Memory,
                "Duration:" + command,
                static_cast<float>(averageDurations[command].count()) / 10000.0f, // Normalize
                0.7f
            );
            
            patternAtom->AddOutgoingLink(durationAtom);
        }
    }
}

void CognitiveProcessMonitor::MonitoringLoop() {
    while (m_monitoring) {
        try {
            // Periodic analysis
            LearnFromProcessPatterns();
            
            // Update all tracked processes
            std::shared_lock<std::shared_mutex> lock(m_processMutex);
            for (const auto& [key, process] : m_trackedProcesses) {
                // Simulate behavior observation
                const_cast<ProcessInfo&>(process).behaviorLog.push_back(
                    "Behavior_" + std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now() - process.startTime).count())
                );
            }
            lock.unlock();
            
            // Sleep for monitoring interval
            std::this_thread::sleep_for(std::chrono::seconds(5));
            
        } catch (...) {
            // Continue monitoring despite errors
        }
    }
}

void CognitiveProcessMonitor::UpdateProcessKnowledge(const ProcessInfo& process) {
    if (!m_integrationManager || !m_integrationManager->GetCognitiveSystem()) return;
    
    auto atomSpace = m_integrationManager->GetCognitiveSystem()->GetGlobalAtomSpace();
    
    if (process.cognitiveRepresentation) {
        // Update attention based on process activity
        float attention = 0.5f + (process.behaviorLog.size() / 100.0f);
        process.cognitiveRepresentation->SetAttention(std::min(1.0f, attention));
        
        // Create behavior memory atoms
        for (const auto& behavior : process.behaviorLog) {
            auto behaviorAtom = atomSpace->CreateAtom(
                cognitive::Atom::Type::Memory,
                process.command + "_behavior:" + behavior,
                0.6f, 0.7f
            );
            process.cognitiveRepresentation->AddOutgoingLink(behaviorAtom);
        }
    }
}

// AutogeneticAgentFactory Implementation
AutogeneticAgentFactory::AutogeneticAgentFactory(std::shared_ptr<CognitiveIntegrationManager> integrationManager)
    : m_integrationManager(integrationManager)
{
    // Register default agent templates
    RegisterAgentTemplate("BasicOptimizer", AgentType::ProcessOptimizer, "Basic process optimization");
    RegisterAgentTemplate("AdvancedResourceManager", AgentType::ResourceManager, "Advanced resource management with predictive capabilities");
    RegisterAgentTemplate("SecurityScanner", AgentType::SecurityAnalyzer, "Real-time security threat detection");
}

AutogeneticAgentFactory::~AutogeneticAgentFactory() {
    // Cleanup will be handled by individual agent destructors
}

std::shared_ptr<cognitive::CognitiveAgent> AutogeneticAgentFactory::CreateAgent(AgentType type, const std::string& name) {
    if (!m_integrationManager || !m_integrationManager->GetCognitiveSystem()) {
        return nullptr;
    }
    
    std::shared_ptr<cognitive::CognitiveAgent> agent;
    
    switch (type) {
        case AgentType::ProcessOptimizer:
            agent = CreateProcessOptimizerAgent(name);
            break;
        case AgentType::ResourceManager:
            agent = CreateResourceManagerAgent(name);
            break;
        case AgentType::SecurityAnalyzer:
            agent = CreateSecurityAnalyzerAgent(name);
            break;
        case AgentType::PerformanceMonitor:
            agent = CreatePerformanceMonitorAgent(name);
            break;
        case AgentType::SystemLearner:
            agent = CreateSystemLearnerAgent(name);
            break;
        case AgentType::AdaptiveScheduler:
            agent = CreateAdaptiveSchedulerAgent(name);
            break;
        default:
            return nullptr;
    }
    
    if (agent) {
        // Enable self-modification by default
        EnableSelfModification(name, true);
        SetSelfModificationParameters(name, 0.01f, "safe_modifications_only");
    }
    
    return agent;
}

std::shared_ptr<cognitive::CognitiveAgent> AutogeneticAgentFactory::CreateCustomAgent(const std::string& name, const std::string& specification) {
    // Create a basic agent and customize based on specification
    auto agent = m_integrationManager->CreateCognitiveAgent(name, "custom");
    
    if (agent) {
        // Parse specification and add appropriate goals and knowledge
        auto atomSpace = m_integrationManager->GetCognitiveSystem()->GetGlobalAtomSpace();
        auto specGoal = atomSpace->CreateAtom(
            cognitive::Atom::Type::Goal,
            "CustomGoal:" + specification,
            0.8f, 0.9f
        );
        agent->AddGoal(specGoal);
    }
    
    return agent;
}

void AutogeneticAgentFactory::RegisterAgentTemplate(const std::string& templateName, AgentType baseType, const std::string& specialization) {
    std::unique_lock<std::shared_mutex> lock(m_templateMutex);
    
    AgentTemplate template_info;
    template_info.baseType = baseType;
    template_info.specialization = specialization;
    
    m_agentTemplates[templateName] = template_info;
}

std::vector<std::string> AutogeneticAgentFactory::GetAvailableTemplates() const {
    std::shared_lock<std::shared_mutex> lock(m_templateMutex);
    
    std::vector<std::string> templates;
    for (const auto& [name, template_info] : m_agentTemplates) {
        templates.push_back(name);
    }
    
    return templates;
}

void AutogeneticAgentFactory::EnableSelfModification(const std::string& agentName, bool enable) {
    std::unique_lock<std::shared_mutex> lock(m_templateMutex);
    m_selfModificationEnabled[agentName] = enable;
}

void AutogeneticAgentFactory::SetSelfModificationParameters(const std::string& agentName, float probability, const std::string& constraints) {
    if (m_integrationManager && m_integrationManager->GetCognitiveSystem()) {
        m_integrationManager->SetCognitiveConfiguration(agentName + "_self_mod_prob", std::to_string(probability));
        m_integrationManager->SetCognitiveConfiguration(agentName + "_self_mod_constraints", constraints);
    }
}

std::shared_ptr<cognitive::CognitiveAgent> AutogeneticAgentFactory::CreateProcessOptimizerAgent(const std::string& name) {
    auto agent = m_integrationManager->CreateCognitiveAgent(name, "optimization");
    
    if (agent) {
        auto atomSpace = m_integrationManager->GetCognitiveSystem()->GetGlobalAtomSpace();
        
        // Add specific goals for process optimization
        auto efficiencyGoal = atomSpace->CreateAtom(cognitive::Atom::Type::Goal, "MaximizeProcessEfficiency", 0.9f, 0.8f);
        auto resourceGoal = atomSpace->CreateAtom(cognitive::Atom::Type::Goal, "MinimizeResourceUsage", 0.8f, 0.9f);
        
        agent->AddGoal(efficiencyGoal);
        agent->AddGoal(resourceGoal);
    }
    
    return agent;
}

std::shared_ptr<cognitive::CognitiveAgent> AutogeneticAgentFactory::CreateResourceManagerAgent(const std::string& name) {
    auto agent = m_integrationManager->CreateCognitiveAgent(name, "resource_management");
    
    if (agent) {
        auto atomSpace = m_integrationManager->GetCognitiveSystem()->GetGlobalAtomSpace();
        
        auto memoryGoal = atomSpace->CreateAtom(cognitive::Atom::Type::Goal, "OptimizeMemoryUsage", 0.8f, 0.9f);
        auto cpuGoal = atomSpace->CreateAtom(cognitive::Atom::Type::Goal, "BalanceCPULoad", 0.8f, 0.9f);
        
        agent->AddGoal(memoryGoal);
        agent->AddGoal(cpuGoal);
    }
    
    return agent;
}

std::shared_ptr<cognitive::CognitiveAgent> AutogeneticAgentFactory::CreateSecurityAnalyzerAgent(const std::string& name) {
    auto agent = m_integrationManager->CreateCognitiveAgent(name, "security");
    
    if (agent) {
        auto atomSpace = m_integrationManager->GetCognitiveSystem()->GetGlobalAtomSpace();
        
        auto threatGoal = atomSpace->CreateAtom(cognitive::Atom::Type::Goal, "DetectThreats", 1.0f, 1.0f);
        auto preventionGoal = atomSpace->CreateAtom(cognitive::Atom::Type::Goal, "PreventIntrusions", 1.0f, 1.0f);
        
        agent->AddGoal(threatGoal);
        agent->AddGoal(preventionGoal);
    }
    
    return agent;
}

std::shared_ptr<cognitive::CognitiveAgent> AutogeneticAgentFactory::CreatePerformanceMonitorAgent(const std::string& name) {
    auto agent = m_integrationManager->CreateCognitiveAgent(name, "monitoring");
    
    if (agent) {
        auto atomSpace = m_integrationManager->GetCognitiveSystem()->GetGlobalAtomSpace();
        
        auto monitorGoal = atomSpace->CreateAtom(cognitive::Atom::Type::Goal, "MonitorPerformance", 0.9f, 0.8f);
        auto alertGoal = atomSpace->CreateAtom(cognitive::Atom::Type::Goal, "AlertOnAnomalies", 0.8f, 0.9f);
        
        agent->AddGoal(monitorGoal);
        agent->AddGoal(alertGoal);
    }
    
    return agent;
}

std::shared_ptr<cognitive::CognitiveAgent> AutogeneticAgentFactory::CreateSystemLearnerAgent(const std::string& name) {
    auto agent = m_integrationManager->CreateCognitiveAgent(name, "learning");
    
    if (agent) {
        auto atomSpace = m_integrationManager->GetCognitiveSystem()->GetGlobalAtomSpace();
        
        auto learnGoal = atomSpace->CreateAtom(cognitive::Atom::Type::Goal, "LearnSystemPatterns", 0.9f, 0.8f);
        auto adaptGoal = atomSpace->CreateAtom(cognitive::Atom::Type::Goal, "AdaptToChanges", 0.8f, 0.9f);
        
        agent->AddGoal(learnGoal);
        agent->AddGoal(adaptGoal);
    }
    
    return agent;
}

std::shared_ptr<cognitive::CognitiveAgent> AutogeneticAgentFactory::CreateAdaptiveSchedulerAgent(const std::string& name) {
    auto agent = m_integrationManager->CreateCognitiveAgent(name, "scheduling");
    
    if (agent) {
        auto atomSpace = m_integrationManager->GetCognitiveSystem()->GetGlobalAtomSpace();
        
        auto scheduleGoal = atomSpace->CreateAtom(cognitive::Atom::Type::Goal, "OptimizeScheduling", 0.8f, 0.9f);
        auto balanceGoal = atomSpace->CreateAtom(cognitive::Atom::Type::Goal, "BalanceWorkload", 0.8f, 0.9f);
        
        agent->AddGoal(scheduleGoal);
        agent->AddGoal(balanceGoal);
    }
    
    return agent;
}

} // namespace wsl::shared::integration