#include "led/LEDLayoutManager.h"
#include <M5Unified.h>
#include <LittleFS.h>
#include <sstream>
#include <cmath>

LEDLayoutManager::LEDLayoutManager() 
    : initialized_(false), led_count_(0) {
    // 32x32 spatial grid initialization
    for (int i = 0; i < SPATIAL_GRID_SIZE; i++) {
        for (int j = 0; j < SPATIAL_GRID_SIZE; j++) {
            spatial_grid_[i][j].clear();
        }
    }
}

LEDLayoutManager::~LEDLayoutManager() {
    // Smart pointers will automatically clean up
}

bool LEDLayoutManager::initialize(const String& csv_file_path) {
    if (initialized_) {
        M5.Log.println("LEDLayoutManager already initialized");
        return true;
    }

    if (!LittleFS.exists(csv_file_path)) {
        M5.Log.printf("LED layout CSV file not found: %s\n", csv_file_path.c_str());
        return false;
    }

    File file = LittleFS.open(csv_file_path, "r");
    if (!file) {
        M5.Log.printf("Failed to open LED layout CSV: %s\n", csv_file_path.c_str());
        return false;
    }

    M5.Log.printf("Loading LED layout from: %s\n", csv_file_path.c_str());
    
    // Clear existing data
    leds_.clear();
    led_count_ = 0;
    
    // Clear spatial grid
    for (int i = 0; i < SPATIAL_GRID_SIZE; i++) {
        for (int j = 0; j < SPATIAL_GRID_SIZE; j++) {
            spatial_grid_[i][j].clear();
        }
    }

    String header = file.readStringUntil('\n');
    M5.Log.printf("CSV Header: %s\n", header.c_str());
    
    uint32_t line_count = 0;
    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        
        if (line.length() == 0) continue;
        
        line_count++;
        
        if (!parseLEDLine(line)) {
            M5.Log.printf("Failed to parse line %u: %s\n", line_count, line.c_str());
            file.close();
            return false;
        }
    }
    
    file.close();
    
    M5.Log.printf("Successfully loaded %u LEDs from CSV\n", led_count_);
    
    // Build spatial index for fast lookup
    buildSpatialIndex();
    
    initialized_ = true;
    return true;
}

bool LEDLayoutManager::parseLEDLine(const String& line) {
    // Expected format: led_id,strip_id,index_in_strip,x,y,z,latitude,longitude
    std::vector<String> fields;
    
    int start = 0;
    int comma_pos;
    while ((comma_pos = line.indexOf(',', start)) != -1) {
        fields.push_back(line.substring(start, comma_pos));
        start = comma_pos + 1;
    }
    fields.push_back(line.substring(start)); // Last field
    
    if (fields.size() != 8) {
        M5.Log.printf("Invalid CSV line format (expected 8 fields, got %d): %s\n", 
                     fields.size(), line.c_str());
        return false;
    }
    
    try {
        LEDPosition led;
        led.led_id = fields[0].toInt();
        led.strip_id = fields[1].toInt();
        led.index_in_strip = fields[2].toInt();
        led.position_3d.x = fields[3].toFloat();
        led.position_3d.y = fields[4].toFloat();
        led.position_3d.z = fields[5].toFloat();
        led.spherical_coords.latitude = fields[6].toFloat();
        led.spherical_coords.longitude = fields[7].toFloat();
        
        // Calculate distance from origin for validation
        float distance = sqrt(led.position_3d.x * led.position_3d.x + 
                             led.position_3d.y * led.position_3d.y + 
                             led.position_3d.z * led.position_3d.z);
        
        // Validate reasonable sphere radius (expecting ~50-200mm)
        if (distance < 30.0f || distance > 300.0f) {
            M5.Log.printf("Warning: LED %d has unusual distance from origin: %.2f\n", 
                         led.led_id, distance);
        }
        
        // Validate strip configuration
        if (led.strip_id < 0 || led.strip_id >= 4) {
            M5.Log.printf("Error: Invalid strip_id %d for LED %d (must be 0-3)\n", 
                         led.strip_id, led.led_id);
            return false;
        }
        
        if (led.index_in_strip < 0 || led.index_in_strip >= 200) {
            M5.Log.printf("Error: Invalid index_in_strip %d for LED %d (must be 0-199)\n", 
                         led.index_in_strip, led.led_id);
            return false;
        }
        
        leds_.push_back(std::make_shared<LEDPosition>(led));
        led_count_++;
        
    } catch (const std::exception& e) {
        M5.Log.printf("Exception parsing CSV line: %s\n", e.what());
        return false;
    }
    
    return true;
}

void LEDLayoutManager::buildSpatialIndex() {
    M5.Log.println("Building spatial index for LED lookup optimization...");
    
    // Find bounding box of all LEDs
    float min_lat = 180.0f, max_lat = -180.0f;
    float min_lon = 180.0f, max_lon = -180.0f;
    
    for (const auto& led : leds_) {
        if (led->spherical_coords.latitude < min_lat) 
            min_lat = led->spherical_coords.latitude;
        if (led->spherical_coords.latitude > max_lat) 
            max_lat = led->spherical_coords.latitude;
        if (led->spherical_coords.longitude < min_lon) 
            min_lon = led->spherical_coords.longitude;
        if (led->spherical_coords.longitude > max_lon) 
            max_lon = led->spherical_coords.longitude;
    }
    
    lat_range_ = {min_lat, max_lat};
    lon_range_ = {min_lon, max_lon};
    
    M5.Log.printf("Spatial bounds: lat[%.2f, %.2f], lon[%.2f, %.2f]\n", 
                 min_lat, max_lat, min_lon, max_lon);
    
    // Populate spatial grid
    for (const auto& led : leds_) {
        int grid_x = static_cast<int>((led->spherical_coords.latitude - min_lat) / 
                                     (max_lat - min_lat) * (SPATIAL_GRID_SIZE - 1));
        int grid_y = static_cast<int>((led->spherical_coords.longitude - min_lon) / 
                                     (max_lon - min_lon) * (SPATIAL_GRID_SIZE - 1));
        
        // Clamp to valid range
        grid_x = constrain(grid_x, 0, SPATIAL_GRID_SIZE - 1);
        grid_y = constrain(grid_y, 0, SPATIAL_GRID_SIZE - 1);
        
        spatial_grid_[grid_x][grid_y].push_back(led);
    }
    
    // Log grid distribution
    int populated_cells = 0;
    int max_cell_count = 0;
    for (int i = 0; i < SPATIAL_GRID_SIZE; i++) {
        for (int j = 0; j < SPATIAL_GRID_SIZE; j++) {
            if (!spatial_grid_[i][j].empty()) {
                populated_cells++;
                if (spatial_grid_[i][j].size() > max_cell_count) {
                    max_cell_count = spatial_grid_[i][j].size();
                }
            }
        }
    }
    
    M5.Log.printf("Spatial index: %d/%d cells populated, max %d LEDs per cell\n", 
                 populated_cells, SPATIAL_GRID_SIZE * SPATIAL_GRID_SIZE, max_cell_count);
}

std::shared_ptr<LEDPosition> LEDLayoutManager::findClosestLED(float latitude, float longitude) const {
    if (!initialized_ || leds_.empty()) {
        return nullptr;
    }
    
    // Use spatial grid for fast lookup
    int grid_x = static_cast<int>((latitude - lat_range_.first) / 
                                 (lat_range_.second - lat_range_.first) * (SPATIAL_GRID_SIZE - 1));
    int grid_y = static_cast<int>((longitude - lon_range_.first) / 
                                 (lon_range_.second - lon_range_.first) * (SPATIAL_GRID_SIZE - 1));
    
    // Clamp to valid range
    grid_x = constrain(grid_x, 0, SPATIAL_GRID_SIZE - 1);
    grid_y = constrain(grid_y, 0, SPATIAL_GRID_SIZE - 1);
    
    std::shared_ptr<LEDPosition> closest_led = nullptr;
    float min_distance = FLT_MAX;
    
    // Search in current cell and neighboring cells
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            int check_x = grid_x + dx;
            int check_y = grid_y + dy;
            
            if (check_x >= 0 && check_x < SPATIAL_GRID_SIZE && 
                check_y >= 0 && check_y < SPATIAL_GRID_SIZE) {
                
                for (const auto& led : spatial_grid_[check_x][check_y]) {
                    float distance = calculateSphericalDistance(
                        latitude, longitude,
                        led->spherical_coords.latitude, 
                        led->spherical_coords.longitude);
                    
                    if (distance < min_distance) {
                        min_distance = distance;
                        closest_led = led;
                    }
                }
            }
        }
    }
    
    return closest_led;
}

std::vector<std::shared_ptr<LEDPosition>> LEDLayoutManager::getLEDsInRadius(
    float center_latitude, float center_longitude, float radius_degrees) const {
    
    std::vector<std::shared_ptr<LEDPosition>> result;
    
    if (!initialized_ || leds_.empty()) {
        return result;
    }
    
    // Calculate grid range to search
    float lat_extent = radius_degrees;
    float lon_extent = radius_degrees;
    
    int min_grid_x = static_cast<int>(((center_latitude - lat_extent) - lat_range_.first) / 
                                     (lat_range_.second - lat_range_.first) * (SPATIAL_GRID_SIZE - 1));
    int max_grid_x = static_cast<int>(((center_latitude + lat_extent) - lat_range_.first) / 
                                     (lat_range_.second - lat_range_.first) * (SPATIAL_GRID_SIZE - 1));
    int min_grid_y = static_cast<int>(((center_longitude - lon_extent) - lon_range_.first) / 
                                     (lon_range_.second - lon_range_.first) * (SPATIAL_GRID_SIZE - 1));
    int max_grid_y = static_cast<int>(((center_longitude + lon_extent) - lon_range_.first) / 
                                     (lon_range_.second - lon_range_.first) * (SPATIAL_GRID_SIZE - 1));
    
    // Clamp to valid range
    min_grid_x = constrain(min_grid_x, 0, SPATIAL_GRID_SIZE - 1);
    max_grid_x = constrain(max_grid_x, 0, SPATIAL_GRID_SIZE - 1);
    min_grid_y = constrain(min_grid_y, 0, SPATIAL_GRID_SIZE - 1);
    max_grid_y = constrain(max_grid_y, 0, SPATIAL_GRID_SIZE - 1);
    
    // Search in grid range
    for (int x = min_grid_x; x <= max_grid_x; x++) {
        for (int y = min_grid_y; y <= max_grid_y; y++) {
            for (const auto& led : spatial_grid_[x][y]) {
                float distance = calculateSphericalDistance(
                    center_latitude, center_longitude,
                    led->spherical_coords.latitude, 
                    led->spherical_coords.longitude);
                
                if (distance <= radius_degrees) {
                    result.push_back(led);
                }
            }
        }
    }
    
    return result;
}

std::vector<std::shared_ptr<LEDPosition>> LEDLayoutManager::getLatitudeLine(float latitude, float tolerance) const {
    std::vector<std::shared_ptr<LEDPosition>> result;
    
    for (const auto& led : leds_) {
        if (abs(led->spherical_coords.latitude - latitude) <= tolerance) {
            result.push_back(led);
        }
    }
    
    // Sort by longitude
    std::sort(result.begin(), result.end(), 
        [](const std::shared_ptr<LEDPosition>& a, const std::shared_ptr<LEDPosition>& b) {
            return a->spherical_coords.longitude < b->spherical_coords.longitude;
        });
    
    return result;
}

std::vector<std::shared_ptr<LEDPosition>> LEDLayoutManager::getLongitudeLine(float longitude, float tolerance) const {
    std::vector<std::shared_ptr<LEDPosition>> result;
    
    for (const auto& led : leds_) {
        if (abs(led->spherical_coords.longitude - longitude) <= tolerance) {
            result.push_back(led);
        }
    }
    
    // Sort by latitude
    std::sort(result.begin(), result.end(), 
        [](const std::shared_ptr<LEDPosition>& a, const std::shared_ptr<LEDPosition>& b) {
            return a->spherical_coords.latitude < b->spherical_coords.latitude;
        });
    
    return result;
}

std::vector<std::shared_ptr<LEDPosition>> LEDLayoutManager::getCoordinateAxisLEDs(float tolerance) const {
    std::vector<std::shared_ptr<LEDPosition>> result;
    
    // X-axis: latitude=0, longitude=0 and longitude=180
    auto x_positive = getLatitudeLine(0.0f, tolerance);
    auto x_negative = getLatitudeLine(0.0f, tolerance);
    
    for (const auto& led : x_positive) {
        if (abs(led->spherical_coords.longitude) <= tolerance || 
            abs(led->spherical_coords.longitude - 180.0f) <= tolerance) {
            result.push_back(led);
        }
    }
    
    // Y-axis: latitude=0, longitude=90 and longitude=-90
    for (const auto& led : x_positive) {
        if (abs(led->spherical_coords.longitude - 90.0f) <= tolerance || 
            abs(led->spherical_coords.longitude + 90.0f) <= tolerance) {
            result.push_back(led);
        }
    }
    
    // Z-axis: latitude=90 and latitude=-90
    auto z_positive = getLatitudeLine(90.0f, tolerance);
    auto z_negative = getLatitudeLine(-90.0f, tolerance);
    
    result.insert(result.end(), z_positive.begin(), z_positive.end());
    result.insert(result.end(), z_negative.begin(), z_negative.end());
    
    return result;
}

uint32_t LEDLayoutManager::getLEDCount() const {
    return led_count_;
}

const std::vector<std::shared_ptr<LEDPosition>>& LEDLayoutManager::getAllLEDs() const {
    return leds_;
}

float LEDLayoutManager::calculateSphericalDistance(float lat1, float lon1, float lat2, float lon2) const {
    // Simple spherical distance approximation (good for small distances)
    float dlat = lat2 - lat1;
    float dlon = lon2 - lon1;
    return sqrt(dlat * dlat + dlon * dlon);
}

bool LEDLayoutManager::isInitialized() const {
    return initialized_;
}