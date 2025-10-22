/*++

Copyright (c) Microsoft. All rights reserved.

Module Name:

    test_cognitive.cpp

Abstract:

    Basic tests for the OpenCog cognitive architecture framework.
    This file provides validation of core cognitive functionality.

--*/

#include "../inc/cognitive.h"
#include "../inc/wsl_cognitive_integration.h"
#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>

namespace wsl::shared::cognitive::test {

void TestAtomCreation() {
    std::cout << "Testing Atom Creation..." << std::endl;
    
    AtomSpace atomSpace;
    
    // Test basic atom creation
    auto concept = atomSpace.CreateAtom(Atom::Type::Concept, "TestConcept", 0.8f, 0.9f);
    assert(concept != nullptr);
    assert(concept->GetType() == Atom::Type::Concept);
    assert(concept->GetName() == "TestConcept");
    assert(concept->GetTruthValue() == 0.8f);
    assert(concept->GetConfidence() == 0.9f);
    
    // Test atom uniqueness by name
    auto concept2 = atomSpace.CreateAtom(Atom::Type::Concept, "TestConcept", 0.5f, 0.7f);
    assert(concept == concept2); // Should return the same atom
    
    // Test different types with same name
    auto process = atomSpace.CreateAtom(Atom::Type::Process, "TestProcess", 0.6f, 0.8f);
    assert(process != concept);
    assert(process->GetName() == "TestProcess");
    
    std::cout << "✓ Atom Creation tests passed" << std::endl;
}

void TestAtomLinks() {
    std::cout << "Testing Atom Links..." << std::endl;
    
    AtomSpace atomSpace;
    
    auto concept1 = atomSpace.CreateAtom(Atom::Type::Concept, "Concept1", 0.8f, 0.9f);
    auto concept2 = atomSpace.CreateAtom(Atom::Type::Concept, "Concept2", 0.7f, 0.8f);
    auto link = atomSpace.CreateAtom(Atom::Type::Link, "TestLink", 0.9f, 0.95f);
    
    // Test linking
    concept1->AddOutgoingLink(link);
    link->AddIncomingLink(concept1);
    link->AddOutgoingLink(concept2);
    concept2->AddIncomingLink(link);
    
    assert(concept1->GetOutgoingLinks().size() == 1);
    assert(concept2->GetIncomingLinks().size() == 1);
    assert(link->GetIncomingLinks().size() == 1);
    assert(link->GetOutgoingLinks().size() == 1);
    
    std::cout << "✓ Atom Links tests passed" << std::endl;
}

void TestAtomSpaceQueries() {
    std::cout << "Testing AtomSpace Queries..." << std::endl;
    
    AtomSpace atomSpace;
    
    // Create test atoms
    auto concept1 = atomSpace.CreateAtom(Atom::Type::Concept, "HighAttention", 0.8f, 0.9f);
    concept1->SetAttention(0.9f);
    
    auto concept2 = atomSpace.CreateAtom(Atom::Type::Concept, "LowAttention", 0.7f, 0.8f);
    concept2->SetAttention(0.2f);
    
    auto process = atomSpace.CreateAtom(Atom::Type::Process, "TestProcess", 0.6f, 0.8f);
    
    // Test type-based queries
    auto concepts = atomSpace.FindAtomsByType(Atom::Type::Concept);
    assert(concepts.size() >= 2); // At least our test concepts plus system concepts
    
    auto processes = atomSpace.FindAtomsByType(Atom::Type::Process);
    assert(processes.size() >= 1);
    
    // Test predicate-based queries
    auto highAttentionAtoms = atomSpace.Query([](const Atom& atom) {
        return atom.GetAttention() > 0.8f;
    });
    
    bool foundHighAttention = false;
    for (const auto& atom : highAttentionAtoms) {
        if (atom->GetName() == "HighAttention") {
            foundHighAttention = true;
            break;
        }
    }
    assert(foundHighAttention);
    
    std::cout << "✓ AtomSpace Queries tests passed" << std::endl;
}

void TestCognitiveAgent() {
    std::cout << "Testing Cognitive Agent..." << std::endl;
    
    auto atomSpace = std::make_shared<AtomSpace>();
    CognitiveAgent agent("TestAgent", atomSpace);
    
    // Test initial state
    assert(agent.GetState() == CognitiveAgent::State::Inactive);
    assert(agent.GetName() == "TestAgent");
    
    // Add a goal
    auto goal = atomSpace->CreateAtom(Atom::Type::Goal, "TestGoal", 0.5f, 0.8f);
    agent.AddGoal(goal);
    assert(agent.GetGoals().size() == 1);
    
    // Test agent lifecycle
    agent.Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Agent should be active and processing
    assert(agent.GetState() != CognitiveAgent::State::Inactive);
    
    agent.Stop();
    assert(agent.GetState() == CognitiveAgent::State::Inactive);
    
    std::cout << "✓ Cognitive Agent tests passed" << std::endl;
}

void TestCognitiveSystem() {
    std::cout << "Testing Cognitive System..." << std::endl;
    
    CognitiveSystem system;
    system.Initialize();
    
    // Test agent creation
    auto agent1 = system.CreateAgent("Agent1");
    assert(agent1 != nullptr);
    assert(agent1->GetName() == "Agent1");
    
    auto agent2 = system.CreateAgent("Agent2");
    assert(agent2 != nullptr);
    assert(agent2->GetName() == "Agent2");
    
    // Test agent retrieval
    auto retrievedAgent = system.GetAgent("Agent1");
    assert(retrievedAgent == agent1);
    
    // Test agent enumeration
    auto agentNames = system.GetAgentNames();
    assert(agentNames.size() >= 2);
    
    // Test statistics
    auto stats = system.GetStatistics();
    assert(stats.totalAgents >= 2);
    assert(stats.totalAtoms > 0);
    
    // Test configuration
    system.SetConfiguration("test_key", "test_value");
    assert(system.GetConfiguration("test_key") == "test_value");
    
    system.Shutdown();
    
    std::cout << "✓ Cognitive System tests passed" << std::endl;
}

void TestIntegrationManager() {
    std::cout << "Testing Integration Manager..." << std::endl;
    
    auto integration = std::make_unique<integration::CognitiveIntegrationManager>();
    bool initialized = integration->Initialize();
    assert(initialized);
    assert(integration->IsInitialized());
    
    // Test cognitive agent creation
    auto agent = integration->CreateCognitiveAgent("TestIntegrationAgent", "testing");
    assert(agent != nullptr);
    
    // Test WSL event handling
    integration->OnProcessCreate("test_distro", 1234, "test_command");
    integration->OnSystemEvent("test_event", "test_data");
    
    // Test querying
    std::string status = integration->QueryCognitiveState("status");
    assert(!status.empty());
    
    // Test statistics
    auto stats = integration->GetStatistics();
    assert(stats.processesMonitored >= 1);
    assert(stats.eventsHandled >= 2);
    
    integration->Shutdown();
    
    std::cout << "✓ Integration Manager tests passed" << std::endl;
}

void TestAgentFactory() {
    std::cout << "Testing Agent Factory..." << std::endl;
    
    auto integration = std::make_shared<integration::CognitiveIntegrationManager>();
    integration->Initialize();
    
    integration::AutogeneticAgentFactory factory(integration);
    
    // Test different agent types
    auto optimizer = factory.CreateAgent(integration::AutogeneticAgentFactory::AgentType::ProcessOptimizer, "TestOptimizer");
    assert(optimizer != nullptr);
    
    auto monitor = factory.CreateAgent(integration::AutogeneticAgentFactory::AgentType::PerformanceMonitor, "TestMonitor");
    assert(monitor != nullptr);
    
    auto security = factory.CreateAgent(integration::AutogeneticAgentFactory::AgentType::SecurityAnalyzer, "TestSecurity");
    assert(security != nullptr);
    
    // Test custom agent
    auto custom = factory.CreateCustomAgent("CustomAgent", "custom functionality");
    assert(custom != nullptr);
    
    // Test templates
    auto templates = factory.GetAvailableTemplates();
    assert(!templates.empty());
    
    integration->Shutdown();
    
    std::cout << "✓ Agent Factory tests passed" << std::endl;
}

void TestProcessMonitor() {
    std::cout << "Testing Process Monitor..." << std::endl;
    
    auto integration = std::make_shared<integration::CognitiveIntegrationManager>();
    integration->Initialize();
    
    integration::CognitiveProcessMonitor monitor(integration);
    
    // Test monitoring lifecycle
    assert(!monitor.IsMonitoring());
    monitor.StartMonitoring();
    assert(monitor.IsMonitoring());
    
    // Test process tracking
    monitor.TrackProcess("test_distro", 1234, "test_command");
    monitor.AnalyzeProcessBehavior("test_distro", 1234);
    
    // Let it run for a short time
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    monitor.UntrackProcess("test_distro", 1234);
    monitor.StopMonitoring();
    assert(!monitor.IsMonitoring());
    
    integration->Shutdown();
    
    std::cout << "✓ Process Monitor tests passed" << std::endl;
}

void TestAttentionSpread() {
    std::cout << "Testing Attention Spread..." << std::endl;
    
    AtomSpace atomSpace;
    
    auto concept1 = atomSpace.CreateAtom(Atom::Type::Concept, "HighAttentionSource", 0.8f, 0.9f);
    auto concept2 = atomSpace.CreateAtom(Atom::Type::Concept, "LinkedConcept", 0.7f, 0.8f);
    
    concept1->SetAttention(1.0f);
    concept2->SetAttention(0.1f);
    
    // Link concepts
    concept1->AddOutgoingLink(concept2);
    
    float initialAttention2 = concept2->GetAttention();
    
    // Update attention values (should spread attention)
    atomSpace.UpdateAttentionValues();
    
    float finalAttention2 = concept2->GetAttention();
    
    // concept2 should have received some attention from concept1
    assert(finalAttention2 > initialAttention2);
    
    std::cout << "✓ Attention Spread tests passed" << std::endl;
}

} // namespace wsl::shared::cognitive::test

int main() {
    using namespace wsl::shared::cognitive::test;
    
    std::cout << "Running OpenCog Cognitive Framework Tests..." << std::endl << std::endl;
    
    try {
        TestAtomCreation();
        TestAtomLinks();
        TestAtomSpaceQueries();
        TestCognitiveAgent();
        TestCognitiveSystem();
        TestIntegrationManager();
        TestAgentFactory();
        TestProcessMonitor();
        TestAttentionSpread();
        
        std::cout << std::endl << "🎉 All tests passed successfully!" << std::endl;
        std::cout << "OpenCog Cognitive Framework is working correctly." << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
        
    } catch (...) {
        std::cerr << "❌ Test failed with unknown exception" << std::endl;
        return 1;
    }
}