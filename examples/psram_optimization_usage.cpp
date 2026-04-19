/**
 * PSRAM Optimization Usage Examples
 *
 * This file demonstrates how to use the PSRAM performance optimization
 * features in your ESP32-S3 application.
 */

#include <Arduino.h>
#include "src/utils/MemoryUtils.h"
#include "src/utils/CachedPSRAMBuffer.h"
#include "src/utils/PSRAMAllocator.h"
#include <vector>
#include <map>

using namespace std;

// Example 1: Basic MemoryUtils Usage
void example_memory_utils() {
    Serial.println("=== Example 1: MemoryUtils Usage ===");

    // Check PSRAM availability
    if (!MemoryUtils::isPSRAMAvailable()) {
        Serial.println("PSRAM not available, using internal RAM only");
        return;
    }

    // Print memory status
    MemoryUtils::printMemoryStatus("example_start");

    // Allocate memory with PSRAM preference
    size_t bufferSize = 8192;
    void* audioData = MemoryUtils::allocateAudioBuffer(bufferSize);
    void* networkData = MemoryUtils::allocateNetworkBuffer(bufferSize);

    if (audioData && networkData) {
        // Use the memory
        memset(audioData, 0, bufferSize);
        memset(networkData, 0xFF, bufferSize);

        Serial.println("Memory allocation successful");
    }

    // Check fragmentation
    size_t fragScore = MemoryUtils::getFragmentationScore();
    Serial.printf("Fragmentation score: %u/100\n", fragScore);

    // Perform defragmentation if needed
    if (MemoryUtils::needsDefragmentation()) {
        Serial.println("Performing defragmentation...");
        MemoryUtils::smartDefragmentPSRAM();
    }

    // Clean up
    if (audioData) free(audioData);
    if (networkData) free(networkData);

    Serial.println();
}

// Example 2: CachedPSRAMBuffer Usage
void example_cached_buffer() {
    Serial.println("=== Example 2: CachedPSRAMBuffer Usage ===");

    // Create a cached buffer for frequently accessed data
    const size_t DATA_SIZE = 4096;
    CachedPSRAMBuffer sensorData(DATA_SIZE, true, 512);

    if (!sensorData.isValid()) {
        Serial.println("Failed to create cached buffer");
        return;
    }

    // Simulate sensor data collection
    for (int reading = 0; reading < 10; reading++) {
        uint8_t sensorReading[256];

        // Generate simulated sensor data
        for (int i = 0; i < 256; i++) {
            sensorReading[i] = (reading * 25 + i) % 256;
        }

        // Write to cached buffer
        size_t offset = (reading * 256) % DATA_SIZE;
        sensorData.write(offset, sensorReading, 256, false);

        // Read back for processing
        uint8_t processed[256];
        sensorData.read(offset, processed, 256);

        // Every 5 readings, sync to PSRAM
        if (reading % 5 == 0 && sensorData.isDirty()) {
            sensorData.syncToPSRAM();
            Serial.printf("Synced sensor data to PSRAM (reading %d)\n", reading);
        }
    }

    // Final sync if needed
    if (sensorData.isDirty()) {
        sensorData.syncToPSRAM();
        Serial.println("Final sync completed");
    }

    // Get memory usage statistics
    size_t psramUsed, cacheUsed;
    sensorData.getMemoryUsage(psramUsed, cacheUsed);
    Serial.printf("Memory usage: PSRAM=%u bytes, Cache=%u bytes\n",
                  psramUsed, cacheUsed);

    Serial.println();
}

// Example 3: PSRAM Allocator with STL Containers
void example_psram_allocator() {
    Serial.println("=== Example 3: PSRAM Allocator Usage ===");

    // Create a large vector using PSRAM
    psram_vector<float> temperatureHistory;

    // Simulate temperature data collection
    for (int hour = 0; hour < 24; hour++) {
        // Simulate temperature reading (20°C ± 5°C)
        float temperature = 20.0f + (sin(hour * 0.2618f) * 5.0f);
        temperatureHistory.push_back(temperature);
    }

    Serial.printf("Collected %u temperature readings\n",
                  (uint32_t)temperatureHistory.size());

    // Calculate statistics
    float sum = 0, minTemp = 100, maxTemp = -100;
    for (float temp : temperatureHistory) {
        sum += temp;
        if (temp < minTemp) minTemp = temp;
        if (temp > maxTemp) maxTemp = temp;
    }

    float avgTemp = sum / temperatureHistory.size();
    Serial.printf("Temperature stats: Avg=%.1f°C, Min=%.1f°C, Max=%.1f°C\n",
                  avgTemp, minTemp, maxTemp);

    // Example with map
    psram_map<string, uint32_t> eventCounts;
    eventCounts["sensor_read"] = 150;
    eventCounts["network_send"] = 42;
    eventCounts["error"] = 3;

    Serial.println("Event counts:");
    for (const auto& pair : eventCounts) {
        Serial.printf("  %s: %u\n", pair.first.c_str(), pair.second);
    }

    Serial.println();
}

// Example 4: Periodic Memory Maintenance
void example_periodic_maintenance() {
    Serial.println("=== Example 4: Periodic Memory Maintenance ===");

    static uint32_t lastMaintenanceTime = 0;
    const uint32_t MAINTENANCE_INTERVAL = 60000; // 60 seconds

    uint32_t currentTime = millis();

    // Check if it's time for maintenance
    if (currentTime - lastMaintenanceTime >= MAINTENANCE_INTERVAL) {
        Serial.println("Performing periodic memory maintenance...");

        // Update timestamp
        lastMaintenanceTime = currentTime;

        // Check and perform defragmentation if needed
        MemoryUtils::periodicDefragmentationCheck();

        // Print current memory status
        MemoryUtils::printMemoryStatus("periodic_maintenance");

        Serial.println("Maintenance completed");
    } else {
        uint32_t remaining = MAINTENANCE_INTERVAL - (currentTime - lastMaintenanceTime);
        Serial.printf("Next maintenance in %u seconds\n", remaining / 1000);
    }

    Serial.println();
}

// Example 5: Real-World Application Pattern
void example_application_pattern() {
    Serial.println("=== Example 5: Application Pattern ===");

    // Configuration: Audio processing application

    // 1. Audio buffer (cached for frequent access)
    static CachedPSRAMBuffer audioBuffer(16384, true, 1024);

    // 2. Audio samples history (large, infrequently accessed)
    static psram_vector<int16_t> audioHistory;

    // 3. Processing statistics (small, frequently updated)
    struct ProcessingStats {
        uint32_t samplesProcessed;
        uint32_t bufferOverflows;
        float averageAmplitude;
    };

    static ProcessingStats stats = {0};

    // Simulate audio processing loop
    for (int frame = 0; frame < 5; frame++) {
        // Simulate audio data arrival
        int16_t audioFrame[512];
        for (int i = 0; i < 512; i++) {
            audioFrame[i] = random(-32768, 32767);
        }

        // Write to cached buffer
        size_t offset = (frame * 512 * sizeof(int16_t)) % audioBuffer.getSize();
        audioBuffer.write(offset, audioFrame, sizeof(audioFrame), false);

        // Add to history
        for (int i = 0; i < 512; i++) {
            audioHistory.push_back(audioFrame[i]);
        }

        // Update statistics
        stats.samplesProcessed += 512;

        // Every 3 frames, sync and check memory
        if (frame % 3 == 0) {
            if (audioBuffer.isDirty()) {
                audioBuffer.syncToPSRAM();
            }

            // Check memory fragmentation periodically
            MemoryUtils::periodicDefragmentationCheck();
        }
    }

    // Print application state
    Serial.printf("Application state:\n");
    Serial.printf("  Audio buffer: %u bytes (%s)\n",
                  audioBuffer.getSize(),
                  audioBuffer.isDirty() ? "needs sync" : "synced");
    Serial.printf("  Audio history: %u samples\n", (uint32_t)audioHistory.size());
    Serial.printf("  Samples processed: %u\n", stats.samplesProcessed);

    Serial.println();
}

// Setup function
void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n========================================");
    Serial.println("PSRAM Optimization Usage Examples");
    Serial.println("========================================\n");

    // Run examples
    example_memory_utils();
    example_cached_buffer();
    example_psram_allocator();
    example_periodic_maintenance();
    example_application_pattern();

    Serial.println("========================================");
    Serial.println("All examples completed successfully!");
    Serial.println("========================================");
}

// Loop function
void loop() {
    // In a real application, you would call periodic maintenance here
    example_periodic_maintenance();

    delay(10000); // Check every 10 seconds
}