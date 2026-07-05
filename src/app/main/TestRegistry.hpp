#pragma once

// Helper macro to register tests
#define REGISTER_TEST(name, func) \
    static bool _##func##_registered = []() { \
        ST::Test::TestManager::get().registerTest(name, []() { func(); }); \
        return true; \
    }()
