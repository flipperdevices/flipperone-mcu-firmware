#pragma once

//https://www.ti.com/lit/ds/symlink/tca6416a.pdf?ts=1762157483597&ref_url=https%253A%252F%252Fwww.ti.com.cn%252Fproduct%252Fcn%252FTCA6416A%252Fpart-details%252FTCA6416APWR

typedef enum {
    input_port_0 = 0x00, /**< Input port 0 */
    input_port_1 = 0x01, /**< Input port 1 */
    output_port_0 = 0x02, /**< Output port 0 */
    output_port_1 = 0x03, /**< Output port 1 */
    polarity_inversion_port_0 = 0x04, /**< Polarity inversion port 0 */
    polarity_inversion_port_1 = 0x05, /**< Polarity inversion port 1 */
    configuration_port_0 = 0x06, /**< Configuration port 0 */
    configuration_port_1 = 0x07, /**< Configuration port 1 */
} Tca6416aReg;
