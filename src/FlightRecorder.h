#pragma once

#include <Arduino.h>
#include <EEPROM.h>
#include <SPIMemory.h>

struct FlashMemoryMap
{
    virtual unsigned char number_of_records() = 0;
    virtual unsigned short number_of_rows_in_record() = 0;
    virtual unsigned long bytes_in_record() = 0;
    virtual unsigned short sector_64k_in_boundary() = 0;
};

struct MemoryMap4M : FlashMemoryMap // Calculated with the Excel Spreadsheet
{
    unsigned char number_of_records() {return 4;};
    unsigned short number_of_rows_in_record() {return 29127;};
    unsigned long bytes_in_record() {return 1048576;};
    unsigned short sector_64k_in_boundary() {return  16;};
};

struct MemoryMap1M : FlashMemoryMap // Calculated with the Excel Spreadsheet
{
    unsigned char number_of_records() {return 2;};
    unsigned short number_of_rows_in_record() {return 14563;};
    unsigned long bytes_in_record() {return 524288;};
    unsigned short sector_64k_in_boundary() {return 8;};
};

enum class EventCode : byte
{
    NO_EVENT = 0b00000000,
    LAUNCHED_OFF_RAIL = 0b00000001,
    APOGEE = 0b00000010,
    LANDED = 0b00000100
};

enum class DepolymentEvent : byte
{
    DEPLOY_NO_EVENT = 0b00000000,
    DEPLOY_MAIN_DEPLOYED = 0b00010000,
    DEPLOY_DROUGUE_DEPLOYED = 0b00100000,
    DEPLOY_MAIN2_DEPLOYED = 0b01000000,
    DEPLOY_DROUGUE2_DEPLOYED = 0b10000000
};

struct alignas(4) FlightInfoFrame
{
    byte deployment_event;
    byte flight_event;
    byte reserved[2];
    float ground_temperature;
    float temperature;
    float ground_pressure;
    float absolute_pressure;
    float acceleration_data[3]; // x, y, z
    uint32_t timestamp;
};

struct alignas(4) RecorderMetadata 
{
    uint8_t current_flight_record = 0;
    uint8_t last_successful_flight = 0;
    uint8_t initialised_val = 221;
    uint8_t reserved;

    bool flight_record_contains_data[4] = {0,0,0,0};
    bool flight_in_progress[4] = {0,0,0,0};
    uint32_t flight_rows_used[4] = {0,0,0,0};

    float max_altitudes[4] = {0,0,0,0};
    unsigned long flight_times[4] = {0,0,0,0};
};

class FlightRecorder
{
protected:
    SPIFlash* flash_;
    int flash_ss_;
    uint32_t flash_size_;
    FlashMemoryMap* memory_map_;
    RecorderMetadata current_metadata_;
    const int eeprom_address_ = 0;

    uint8_t active_flight_record_ = 255;
    uint32_t current_write_idx_ = 0;
    uint32_t current_write_row_idx_ = 0;
    uint32_t start_flight_time_ = 0;

    float min_pressure_;
    float start_pressure_;
    float start_temperature_;

    bool storeBytes(uint32_t start_address, void* data, size_t data_size);
    bool readBytes(uint32_t start_address, void* data, size_t data_size);

    bool erase64KSector(uint32_t sector);

public:
    FlightRecorder(uint32_t flash_size, int flash_ss_pin, FlashMemoryMap& memory_map);
    bool begin();

    void saveMetadata();
    void loadMetadata();
    RecorderMetadata getMetadata();

    byte beginFlight(bool override); // returns the flight number used
    byte saveDataframe(FlightInfoFrame data);
    byte endFlight();

    float calculateAltitude(float current_pressure, float ground_pressure, float ground_temperature);
    
    uint8_t getActiveFlightRecord();
    uint16_t getCurrentRowIdx();
    float getMinimumPressure();
    float getStartPressure();
    float getStartTemperature();

    bool loadDataframe(uint8_t flight_number, uint32_t row_index, FlightInfoFrame* data_frame_out);
    bool eraseFlightRecord(uint8_t flight_number);
};
