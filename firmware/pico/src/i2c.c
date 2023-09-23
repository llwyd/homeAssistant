#include "i2c.h"

#define I2C_BAUDRATE ( 400000U )
#define I2C_TIMEOUT  ( 500000U )

void I2C_Init(void)
{
    printf("Initialising I2C Bus\n");
    i2c_init(i2c_default, I2C_BAUDRATE );    
}

int8_t I2C_ReadReg(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
    uint8_t address = *(uint8_t *)intf_ptr;
    int8_t rslt = 0U;

    //printf("Expecting to read %d bytes, ", len);
    int ret0 = i2c_write_blocking( i2c_default,
                                    address,
                                    &reg_addr,
                                    1U,
                                    true
                                     );
    if( ret0 < 0 )
    {
        printf("\tI2C Read(W) fail\n");
        rslt = -1;
        goto cleanup;
    }
   
    int ret1 = i2c_read_blocking(i2c_default,address,reg_data,len,false);
    
    if( ret1 < 0 )
    {
        printf("\tI2C Read fail\n");
        rslt = -1;
        goto cleanup;
    }
    else
    {
        //printf("%d bytes read\n", ret1);
    }
    
cleanup:
    return rslt;
}

int8_t I2C_ReadRegQuiet(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
    uint8_t address = *(uint8_t *)intf_ptr;
    int8_t rslt = 0U;

    //printf("Expecting to read %d bytes, ", len);
    int ret0 = i2c_write_blocking( i2c_default,
                                    address,
                                    &reg_addr,
                                    1U,
                                    true
                                     );
    if( ret0 < 0 )
    {
        rslt = -1;
        goto cleanup;
    }
   
    int ret1 = i2c_read_blocking(i2c_default,address,reg_data,len,false);
    
    if( ret1 < 0 )
    {
        rslt = -1;
        goto cleanup;
    }
    else
    {
        //printf("%d bytes read\n", ret1);
    }
    
cleanup:
    return rslt;
}

int8_t I2C_WriteReg(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
    uint8_t address = *(uint8_t *)intf_ptr;
    int8_t rslt = 0U;
    uint8_t buffer[32] = {0};
    memset(buffer, 0x00, 32);

    buffer[0] = reg_addr;
    memcpy(&buffer[1], reg_data, len );
    //printf("Expecting to write %d bytes, ", len + 1U);
     
    int ret = i2c_write_blocking( i2c_default,
                                    address,
                                    buffer,
                                    len + 1U,
                                    false
                                     );
    if( ret < 0 )
    {
        printf("\tI2C Write fail\n");
        rslt = -1;
        goto cleanup;
    }
    else
    {
        //printf("%d bytes written\n", ret);
    }
    
cleanup:
    return rslt;
}
