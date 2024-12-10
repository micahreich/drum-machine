#ifndef SHIFT_REGISTER_DEBOUNCE_H
#define SHIFT_REGISTER_DEBOUNCE_H

#include "Bounce2.h"

class ShiftRegisterBounce : public Debouncer {
public:
    /*!
     * @brief Default Constructor for ShiftRegisterBounce
     */
    ShiftRegisterBounce() : shift_register_data(NULL), button_index(0) {};

    /*!
     * @brief Constructor for ShiftRegisterBounce
     * 
     * @param shift_register_data Reference to an array or value representing shift register output bits
     * @param button_index Index of the button in the shift register to be debounced
     */
    ShiftRegisterBounce(uint8_t *shift_register_data, uint8_t button_index)
        : shift_register_data(shift_register_data), button_index(button_index) {}

    /*!
     * @brief Updates the button index. Use this if buttons are dynamically reassigned.
     * 
     * @param newIndex New button index to be monitored
     */
    void setButtonIndex(uint8_t newIndex) {
        button_index = newIndex;
    }

    /*!
     * @brief Updates the shift register data. Use this if the shift register data is dynamically reassigned.
     * 
     * @param newData New shift register data to be monitored
     */
    void setShiftRegisterData(uint8_t *newData) {
        shift_register_data = newData;
    }
    
protected:
    /*!
     * @brief Reads the current state of the button from the shift register.
     * 
     * @return true if the button is pressed, false otherwise.
     */
    virtual bool readCurrentState() override {
        uint8_t byte_index = button_index / 8;  // Determine which byte in the shift register contains the button bit
        uint8_t bit_mask = 1 << (button_index % 8);  // Mask for the specific bit
        return (shift_register_data[byte_index] & bit_mask) != 0;
    }

private:
    uint8_t *shift_register_data;  // Pointer to the shift register output array
    uint8_t button_index;         // Index of the button in the shift register
};

#endif  // SHIFT_REGISTER_DEBOUNCE_H