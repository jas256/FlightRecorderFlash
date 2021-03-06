#include <FlightRecorder.h>

FlightRecorder::FlightRecorder(uint32_t flash_size, int flash_ss_pin, FlashMemoryMap &memory_map) : flash_size_(flash_size)
{
    flash_ = new SPIFlash(flash_ss_pin);
    memory_map_ = &memory_map;
}

void FlightRecorder::loadMetadata()
{
    EEPROM.get(eeprom_address_, current_metadata_);
}

void FlightRecorder::saveMetadata()
{
    EEPROM.put(eeprom_address_, current_metadata_);
#if defined(ARDUINO_ARCH_AVR)
#else
    EEPROM.commit();
#endif  
}

byte FlightRecorder::saveDataframe(FlightInfoFrame data)
{
    uint32_t flight_start_address = (active_flight_record_ * memory_map_->bytes_in_record());
    uint32_t write_address = flight_start_address + current_write_idx_;
    uint32_t final_address = flight_start_address + current_write_idx_ + sizeof(data);

    if (active_flight_record_ == 255) // Flight Not Started
        return 0x01;

    if (final_address > (active_flight_record_ * memory_map_->bytes_in_record()) + (memory_map_->bytes_in_record() - 1)) // Out of space
    {
        endFlight();
        return 0x02;
    }

    if (current_write_row_idx_ == 0) // first entry
    {
        start_flight_time_ = data.timestamp;
        min_pressure_ = data.absolute_pressure;
        start_pressure_ = data.ground_pressure;
        start_temperature_ = data.ground_temperature;
    }
    data.timestamp = data.timestamp - start_flight_time_;
    bool success = storeBytes(write_address, &data, sizeof(data));
    if (success)
    {
        if (data.absolute_pressure < min_pressure_)
        {
            min_pressure_ = data.absolute_pressure;
        }
        current_write_idx_ += sizeof(data);
        current_write_row_idx_++;

        current_metadata_.flight_times[active_flight_record_] = data.timestamp;
        current_metadata_.flight_rows_used[active_flight_record_] = current_write_row_idx_;
        return 0x00; // Success
    }
    return 0x03; // Write Error
}

bool FlightRecorder::loadDataframe(uint8_t flight_number, uint32_t row_index, FlightInfoFrame *data_frame_out)
{
    uint32_t flight_start_address = (flight_number * memory_map_->bytes_in_record());
    uint32_t read_address = flight_start_address + (row_index * sizeof(FlightInfoFrame));
    bool success = readBytes(read_address, data_frame_out, sizeof(FlightInfoFrame));
    return success;
}

byte FlightRecorder::beginFlight(bool override = false)
{
    if (current_metadata_.flight_record_contains_data[current_metadata_.current_flight_record] && override == false)
    {
        return 254;
    }
    if (!eraseFlightRecord(current_metadata_.current_flight_record))
    {
        return 253;
    }
    active_flight_record_ = current_metadata_.current_flight_record;
    if (current_metadata_.current_flight_record + 1 >= memory_map_->number_of_records())
    {
        current_metadata_.current_flight_record = 0;
    }
    else
    {
        current_metadata_.current_flight_record++;
    }

    current_metadata_.flight_in_progress[active_flight_record_] = true;
    current_metadata_.flight_record_contains_data[active_flight_record_] = true;
    saveMetadata();
    return active_flight_record_;
}

byte FlightRecorder::endFlight()
{
    current_metadata_.flight_in_progress[active_flight_record_] = false;
    current_metadata_.max_altitudes[active_flight_record_] = calculateAltitude(min_pressure_, start_pressure_, start_temperature_);
    current_metadata_.last_successful_flight = active_flight_record_;
    saveMetadata();
    start_flight_time_ = 0;
    current_write_idx_ = 0;
    current_write_row_idx_ = 0;
    active_flight_record_ = 255;

    return 0x00;
}

bool FlightRecorder::storeBytes(uint32_t start_address, void *data, size_t data_size)
{
    return flash_->writeByteArray(start_address, static_cast<uint8_t *>(data), data_size);
}

bool FlightRecorder::readBytes(uint32_t start_address, void *data, size_t data_size)
{
    return flash_->readByteArray(start_address, static_cast<uint8_t *>(data), data_size);
}

bool FlightRecorder::erase64KSector(uint32_t sector)
{
    return flash_->eraseBlock64K((sector * 64 * 1024) - 1);
}

bool FlightRecorder::eraseFlightRecord(uint8_t flight_number)
{
    uint16_t start_sector_erase_boundary = flight_number * memory_map_->sector_64k_in_boundary();
    uint16_t end_sector_erase_boundary = (flight_number * memory_map_->sector_64k_in_boundary()) + (memory_map_->sector_64k_in_boundary() - 1);
    for (uint16_t i = start_sector_erase_boundary; i < end_sector_erase_boundary; i++)
    {
        if (!erase64KSector(i))
        {
            return false;
        }
    }
    return true;
}

bool FlightRecorder::begin()
{
#if defined(ARDUINO_ARCH_AVR)
    EEPROM.begin();
#else
    EEPROM.begin(1024);
#endif
    loadMetadata();
    if (current_metadata_.initialised_val != 221)
    {
        RecorderMetadata reset_metadata;
        EEPROM.put(eeprom_address_, reset_metadata);
#if defined(ARDUINO_ARCH_AVR)
#else
        EEPROM.commit();
#endif
        loadMetadata();
    }
    return flash_->begin(flash_size_);
}

RecorderMetadata FlightRecorder::getMetadata()
{
    return current_metadata_;
}

float FlightRecorder::calculateAltitude(float current_pressure, float ground_pressure, float ground_temperature)
{
    // From https://www.mide.com/air-pressure-at-altitude-calculator
    float temp_kelvin = ground_temperature + 273.15;
    float tb_lb = temp_kelvin / -0.0065;
    const float constant_exponent = 0.1902632365; // calculated from (-8.31432 * -0.0065)/(9.80665*0.0289644)
    float p_pb = pow((current_pressure / ground_pressure), constant_exponent) - 1;
    return tb_lb * p_pb;
}

uint8_t FlightRecorder::getActiveFlightRecord()
{
    return active_flight_record_;
}

uint16_t FlightRecorder::getCurrentRowIdx()
{
    return current_write_row_idx_;
}

float FlightRecorder::getMinimumPressure()
{
    return min_pressure_;
}

float FlightRecorder::getStartPressure()
{
    return start_pressure_;
}

float FlightRecorder::getStartTemperature()
{
    return start_temperature_;
}
