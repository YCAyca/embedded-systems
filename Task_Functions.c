/* This file includes the robot platform's component's task functions. 
   These functions determine which data will be transmitted from microprocessor 
   to the sensor having I2C Bus or  which data to be taken from the related register 
   of this sensor. In this way, the operating settings of the respective sensor are made,
   changed if necessary and the data generated by the sensor is received for use in the user interface.
   All these arrangements are made by looking the related datasheets of the models of components.
   Led Driver Model : TEXAS INSTRUMENTS LP55231 
   Ioex Driver Model : Toshiba PCAL6416AHF,128 
   Proxy & Color Sensor Model : AMS TMG39923
   Acc & Gyro Sensor Model : Bosch BMI160 
*/

void LedDriver_Tasks(void)
{
    static bool cw_done = false;
    static bool chip_disabled = false, chip_renabled = false;
    
    switch(leddrvStates)
    {
        case CHIP_ENABLE :
        {
            if (chip_disabled)
                chip_renabled = true;
            STATUS_FLAGS.leddrivertasks = 1;
            I2C_TX_SIZE = 2;
            mikropSlaveAddress = 0x64;
            I2CWriteString[0] = 0;
            I2CWriteString[1] = 0x40 ;
            i2cStates = I2C_TRANSMIT;
            leddrvStates = MISC;
            break;
        }
        case CHIP_DISABLE :
        {
            chip_disabled = true;
            STATUS_FLAGS.leddrivertasks = 1;
            I2C_TX_SIZE = 2;
            mikropSlaveAddress = 0x64;
            I2CWriteString[0] = 0;
            I2CWriteString[1] = 0x00;
            i2cStates = I2C_TRANSMIT;
            leddrvStates = LEDDONE;
            break;
        }
        
        case MISC:
        {           
            I2CWriteString[0] = 0x36;
            I2CWriteString[1] = 0x53;
            i2cStates = I2C_TRANSMIT;
            leddrvStates = LEDS_ON_OFF;
            break;
        }
        case LEDS_ON_OFF:
        {       
            i2cStates = I2C_TRANSMIT;
            I2C_TX_SIZE = 3;
            I2CWriteString[0] = 0x04;
            I2CWriteString[1] = 0x01;
            I2CWriteString[2] = 0xFF;
            leddrvStates = LEDPWM;
            break;
        }
        case LEDPWM:
        {
            isleddrvconfigured = false;
            STATUS_FLAGS.leddrivertasks = 1;
            if(IsAnyLEDChange() || !ld_initialized || chip_renabled)
            {   
                i2cStates = I2C_TRANSMIT;
                cw_done = true;
                chip_renabled = false;
                I2C_TX_SIZE = 10;
                mikropSlaveAddress = 0x64;
                I2CWriteString[0] = 0x16;   
                I2CWriteString[1] = RGBtop.green;
                I2CWriteString[2] = RGBtop.blue;
                I2CWriteString[3] = RGBbottom.green;
                I2CWriteString[4] = RGBbottom.blue;
                I2CWriteString[5] = Led_Headlight;
                I2CWriteString[6] = Led_User;
                I2CWriteString[7] = RGBtop.red;
                I2CWriteString[8] = RGBbottom.red;
                I2CWriteString[9] = Led_Power;
                
            }
            leddrvStates = LEDDONE;
            break;
        }
        case LEDDONE:
        {    
            if(cw_done)
            {
                RGBtop_tmp.topRGB = RGBtop.topRGB;
                RGBbottom_tmp.bottomRGB = RGBbottom.bottomRGB;
                Led_Headlight_tmp = Led_Headlight;
                Led_User_tmp = Led_User;
                cw_done = false;   
            }    
            isleddrvconfigured = true;
            STATUS_FLAGS.leddrivertasks = 0;
            ld_initialized = true;
            leddrvStates = LEDPWM;
            break;
        }
    }
}

bool IOEX_Tasks(void)
{
    switch(ioexStates)
    {
        case INIT:
        {
            STATUS_FLAGS.ioextasks = 1;
            mikropSlaveAddress = 0x40;
            I2C_TX_SIZE = 3;
            I2CWriteString[0] = 0x06;
            I2CWriteString[1] = 0x00;
            I2CWriteString[2] = 0x00;
            i2cStates = I2C_TRANSMIT;
            ioexStates = SET;
            return false;
        }
        case SET:
        {   
            isioexconfigured = false;
            STATUS_FLAGS.ioextasks = 1;
            if(IsAnyMOTORChange() || !ioex_initialized)
            {
                i2cStates = I2C_TRANSMIT;
                mikropSlaveAddress = 0x40;
                I2C_TX_SIZE = 3;
                I2CWriteString[0] = 0x02;
                I2CWriteString[1] = IOX_P0.PORT0 ;
                I2CWriteString[2] = IOX_P1.PORT1 ;
                if(CONTROL_BITS2.ioex_interrupt)
                    ioexStates = IOEXDONE;
                else
                    ioexStates = READ_PINS;
            }
            else
                ioexStates = IOEXDONE;
            return false;
        }
        case READ_PINS :
        {
            mikropSlaveAddress = 0x40;
            I2C_TX_SIZE = 1;
            I2C_RX_SIZE = 2;
            I2CWriteString[0] = 0x02;
            i2cStates = I2C_TRANSMITTHENRECEIVE;
            ioexStates = PINS_READ;
            return false;
        }    
        case PINS_READ :
        {
            IOX_P0_tmp.PORT0 = I2CReadString[0];
            IOX_P1_tmp.PORT1 = I2CReadString[1];
            ioexStates = IOEXDONE;
            return false;
        }    
        case IOEXDONE:
        {            
            IRDA_PORTS.IRDA1 =!IOX_P0_tmp.IREN1;
            IRDA_PORTS.IRDA2 =!IOX_P0_tmp.IREN2;
            IRDA_PORTS.IRDA3 =!IOX_P1_tmp.IREN3;
            CONTROL_BITS3.wifi_engine_enabled = IOX_P0_tmp.WEN;
            isioexconfigured = true;
            STATUS_FLAGS.ioextasks = 0;
            ioexStates = SET; 
            ioex_initialized = true;
            return true;
        }
    }
}

void PC_Sensor_Tasks(void)
{
    static bool irda_kill_flag = false;
    switch(sensorStates)
    {
        case SENSOR_PON:
        {
            STATUS_FLAGS.pcsensortasks = 1;
            issensorconfigured = false;
           
            if (CONTROL_BITS.ask_for_pen == 1 && U1STAbits.RIDLE && !CONTROL_BITS.sensor_proxy_restriction)
            {    
                PRSENSOR_ENABLE.pen = 1;
                if (Which_IRDA_Port() == 1)
                {
                    Close_IRDA_Ports();
                    sensorStates = IRDA_FRONT_KILL;
                    irda_kill_flag = true;
                    break;
                }    
            }
            mikropSlaveAddress = 0x72;
            I2C_TX_SIZE = 2;
            I2CWriteString[0] = 0x80;
            I2CWriteString[1] = PRSENSOR_ENABLE.enableRegister;
            if(!PRSENSOR_ENABLE.pon)
               sensorStates = SENSOR_DONE;
            else
               sensorStates = SENSOR_CONT_REG;
            i2cStates = I2C_TRANSMIT;
            break;
        }
        case IRDA_FRONT_KILL :
        {
            mikropSlaveAddress = 0x40;
            I2C_TX_SIZE = 3;
            I2CWriteString[0] = 0x02;
            I2CWriteString[1] = IOX_P0.PORT0;
            I2CWriteString[2] = IOX_P1.PORT1;
            i2cStates = I2C_TRANSMIT;
            sensorStates = SENSOR_PROXY_OPEN;
            break;
        }         
        case SENSOR_PROXY_OPEN :
        {
            mikropSlaveAddress = 0x72;
            I2C_TX_SIZE = 2;
            I2CWriteString[0] = 0x80;
            I2CWriteString[1] = PRSENSOR_ENABLE.enableRegister;
            i2cStates = I2C_TRANSMIT;
            sensorStates = SENSOR_CONT_REG;
            break;
        }        
        case SENSOR_CONT_REG:
        {
            mikropSlaveAddress = 0x72;
            I2C_TX_SIZE = 2;
            I2CWriteString[0] = 0x8F;
            I2CWriteString[1] = 0x0F; // proxy : 8x als : 64x gain
            if(PRSENSOR_ENABLE.pen)  
            {
                sensorSF = SF_PROXY;
                sensorStates = SENSOR_READ_PROXY;
            } 
            else if(PRSENSOR_ENABLE.aen)
            {
                sensorSF = SF_ALS;
                sensorStates = SENSOR_READ_ALS;
            }
            i2cStates = I2C_TRANSMIT;
            break;
        }
        case SENSOR_READ_PROXY:
        {
            mikropSlaveAddress = 0x72;
            I2C_TX_SIZE = 1;
            I2C_RX_SIZE = 1;
            I2CWriteString[0] = 0x9C;
            i2cStates = I2C_TRANSMITTHENRECEIVE;
            sensorStates = SENSOR_DATA_READ;
            break;
        }
        case SENSOR_READ_ALS:
        {
            mikropSlaveAddress = 0x72;
            I2C_TX_SIZE=1;
            I2C_RX_SIZE=8;
            I2CWriteString[0] = 0x94;
            i2cStates = I2C_TRANSMITTHENRECEIVE;
            sensorStates = SENSOR_DATA_READ;
            break;
        }
        case SENSOR_DATA_READ:
        {
            switch(sensorSF)
            {
                case SF_PROXY:
                {
                    SDAT.dataProxy =  I2CReadString[0];
                    if(PRSENSOR_ENABLE.aen)
                    { 
                        sensorSF = SF_ALS;
                        sensorStates = SENSOR_READ_ALS;
                    }
                    else
                        sensorStates = SENSOR_PROXY_KILL;
                    break;
                }
                case SF_ALS:
                { 
                    SDAT.dataALSclear.clearL = I2CReadString[0];
                    SDAT.dataALSclear.clearH = I2CReadString[1];
                    SDAT.dataALSred.redL = I2CReadString[2];
                    SDAT.dataALSred.redH = I2CReadString[3];
                    SDAT.dataALSgreen.greenL = I2CReadString[4];
                    SDAT.dataALSgreen.greenH = I2CReadString[5];
                    SDAT.dataALSblue.blueL = I2CReadString[6];
                    SDAT.dataALSblue.blueH = I2CReadString[7];
                    
                    if (PRSENSOR_ENABLE.pen)
                        sensorStates = SENSOR_PROXY_KILL;
                    else
                    {    
                        SDAT.dataProxy = UNKNOWN_PROXY; // bu tur proxy okunmad?, bi önceki turdan kalan okunmu? de?eri güncel de?er olarak almamal? kullan?c?
                        sensorStates = SENSOR_DONE;
                    }    
                    break;
                }
            }
            break;
        }
        
        case SENSOR_PROXY_KILL:
        {
            i2cStates = I2C_TRANSMIT;
            PRSENSOR_ENABLE.pen = 0;
            mikropSlaveAddress = 0x72;
            I2C_TX_SIZE = 2;
            I2CWriteString[0] = 0x80;
            I2CWriteString[1] = PRSENSOR_ENABLE.enableRegister;
            if(irda_kill_flag)
               sensorStates = IRDA_REOPEN;
            else
               sensorStates = SENSOR_DONE; 
            break;
        }    
        
        case IRDA_REOPEN:
        {
            IRDA_Port_Select(1);
            mikropSlaveAddress = 0x40;
            I2C_TX_SIZE = 3;
            I2CWriteString[0] = 0x02;
            I2CWriteString[1] = IOX_P0.PORT0 ;
            I2CWriteString[2] = IOX_P1.PORT1 ;
            irda_kill_flag = false;
            i2cStates = I2C_TRANSMIT;
            sensorStates = SENSOR_DONE;
            break;
        }    
        
        case SENSOR_DONE:
        {
            
            issensorconfigured = true;
            STATUS_FLAGS.pcsensortasks = 0;
            sensorStates = SENSOR_PON;
            break;
        }
    }
}


void Acc_Gyro_Tasks(void)
{
    static bool tap_int_en = 0;
    static bool conf_change = false;
    
    switch (accgyroStates)
    {
        case ACC_CONF:
        {
            STATUS_FLAGS.accgyrotasks = 1;
            isaccgyroconfigured = false;
            mikropSlaveAddress = 0xD0;
            I2C_TX_SIZE = 3;
            I2CWriteString[0] = 0x40;
            I2CWriteString[1] = 0x2B; 
            if(conf_change)
            {    
                I2CWriteString[2] = 0x0C; // +- 16g
                accgyroStates = ACG_READ_STATUS;    
            }
            else
            {
                I2CWriteString[2] =  0x08; // +- 8g
                accgyroStates = GYR_CONF;
            }
                i2cStates = I2C_TRANSMIT;
            
            
            break;
        }        
        case GYR_CONF:
        {
            mikropSlaveAddress = 0xD0;
            I2C_TX_SIZE = 3;
            I2CWriteString[0] = 0x42;
            I2CWriteString[1] = 0x2B; 
            I2CWriteString[2] = 0x08; 
            i2cStates = I2C_TRANSMIT;
            accgyroStates = ACC_PMU ;  
            break;
        }
        
        case ACC_PMU:
        {
            mikropSlaveAddress = 0xD0;
            I2C_TX_SIZE = 2;
            I2CWriteString[0] = 0x7E;
            I2CWriteString[1] = 0x11; 
            i2cStates = I2C_TRANSMIT;
            accgyroStates = GYR_PMU ;
            break;
        }
        case GYR_PMU:
        {
            uint16_t waitgyro = 0;
            mikropSlaveAddress = 0xD0;
            for(waitgyro = 0; waitgyro < 60000; waitgyro++)
            {
                Nop();
            }
            I2C_TX_SIZE = 2;
            I2CWriteString[0] = 0x7E;
            I2CWriteString[1] = 0x15;   // gyro normal modda. 
            i2cStates = I2C_TRANSMIT;
            accgyroStates = INT_EN;            
            break;
        }
        case INT_EN :
        {
            mikropSlaveAddress = 0xD0;
            I2C_TX_SIZE = 4;
            I2CWriteString[0] = 0x50;
            I2CWriteString[1] = 0xF0; // flat, orient, single/double taps enabled
            I2CWriteString[2] = 0x07; // high g enabled
            I2CWriteString[3] = 0x0F; // step detection, slow/no motion enabled
            i2cStates = I2C_TRANSMIT;
            accgyroStates = INT_CONF1;    
            break;
        }
        case INT_CONF1 : 
        {
            mikropSlaveAddress = 0xD0;
            I2C_TX_SIZE = 9;
            I2CWriteString[0] = 0x5D;
            I2CWriteString[1] = 0x03; // 5D HIGH_G
            I2CWriteString[2] = 0x15; // 5E HIGH_G_ 
            I2CWriteString[3] = 0x0C; // 5F NO_MOT
            I2CWriteString[4] = 0x00; // 60 - (ANY ))
            I2CWriteString[5] = 0x00; // 61 NO_MOT
            I2CWriteString[6] = 0x01; // 62 NO_MOT
            I2CWriteString[7] = 0x00; // 63 TAP
            I2CWriteString[8] = 0x00; // 64 TAP
            i2cStates = I2C_TRANSMIT;
            accgyroStates = INT_CONF2;    
            break;
        } 
        case INT_CONF2 : 
        {
            mikropSlaveAddress = 0xD0;
            I2C_TX_SIZE = 3;
            I2CWriteString[0] = 0x67;
            I2CWriteString[1] = 0x01; // 67 FLAT
            I2CWriteString[2] = 0x05; // 68 FLAT
            i2cStates = I2C_TRANSMIT;
            accgyroStates = ACG_READ_STATUS;    
            break;
        }
        case ACG_READ_STATUS:
        {
            STATUS_FLAGS.accgyrotasks = 1;
            isaccgyroconfigured = false;
            if (!tap_int_en && CONTROL_BITS.tap_interrupt_enable) // tap mode opened. change acc range configuration
            {
                conf_change = true;
                IOX_P0.MEN = 0;
                CONTROL_BITS.motor_queue_permission = 0;
                CONTROL_BITS.acc_gyr_constraint = 1;
                tap_int_en = CONTROL_BITS.tap_interrupt_enable;
                accgyroStates = ACC_CONF;
                break;
            }  
            else if(tap_int_en && !CONTROL_BITS.tap_interrupt_enable) // tap mode closed. change acc range configuration
            {
                conf_change = false;
                IOX_P0.MEN = 1;
                CONTROL_BITS.motor_queue_permission = 1;
                CONTROL_BITS.acc_gyr_constraint = 0;
                tap_int_en = CONTROL_BITS.tap_interrupt_enable;
                accgyroStates = ACC_CONF;
                break;
            }
            
           
            mikropSlaveAddress = 0xD0;
            I2C_TX_SIZE = 1;
            I2C_RX_SIZE = 1;
            I2CWriteString[0] = 0x1B;           
            i2cStates = I2C_TRANSMITTHENRECEIVE;
            accgyroStates = ACG_STATUS_READ;
            break;
        }
        case ACG_STATUS_READ:
        {
            ACG_STATUS.acgstatus =  I2CReadString[0];
            if(ACG_STATUS.drdy_acc && ACG_STATUS.drdy_gyro)
                accgyroStates = READ_AGDATA;
            else
                accgyroStates = ACG_READ_STATUS;
            break;
        }
            
            
        case READ_AGDATA:
        {
            mikropSlaveAddress = 0xD0;
            I2C_TX_SIZE = 1;
            I2C_RX_SIZE = 15; 
            I2CWriteString[0] = 0x0C;
            i2cStates = I2C_TRANSMITTHENRECEIVE;
            accgyroStates = AGDATA_READ;     
            break;
        }
        case AGDATA_READ:
        {
            gyroXdata.gyroLX = I2CReadString[0];
            gyroXdata.gyroHX = I2CReadString[1];
            gyroYdata.gyroLY = I2CReadString[2];
            gyroYdata.gyroHY = I2CReadString[3];
            gyroZdata.gyroLZ = I2CReadString[4];
            gyroZdata.gyroHZ = I2CReadString[5];
            accXdata.accLX = I2CReadString[6];
            accXdata.accHX = I2CReadString[7];
            accYdata.accLY = I2CReadString[8];
            accYdata.accHY = I2CReadString[9];
            accZdata.accLZ = I2CReadString[10];
            accZdata.accHZ = I2CReadString[11];
            acg_time_stamp.sensortime0 = I2CReadString[12];
            acg_time_stamp.sensortime1 = I2CReadString[13];
            acg_time_stamp.sensortime2 = I2CReadString[14];    
            accgyroStates = READ_INTERRUPTS;
            break;
        }
        
        case READ_INTERRUPTS:
        {
            mikropSlaveAddress = 0xD0;
            I2C_TX_SIZE = 1;
            I2C_RX_SIZE = 6;
            I2CWriteString[0] = 0x1C;
            i2cStates = I2C_TRANSMITTHENRECEIVE;
            accgyroStates = INTERRUPTS_READ;
            break;
        }    
        case INTERRUPTS_READ:
        {
            ACG_INTERRUPT_STATUS0.acginterrupt_status0 = I2CReadString[0];
            ACG_INTERRUPT_STATUS1.acginterrupt_status1 = I2CReadString[1];
            ACG_INTERRUPT_STATUS2.acginterrupt_status2 = I2CReadString[2];
            ACG_INTERRUPT_STATUS3.acginterrupt_status3 = I2CReadString[3]; 
            temperature.tempL = I2CReadString[4]; 
            temperature.tempH = I2CReadString[5]; 
            accgyroStates = ACC_GYRO_DONE;
            break;
        }    
      
        case ACC_GYRO_DONE:
        {
            isaccgyroconfigured = true;
            accgyroStates = ACG_READ_STATUS;
            STATUS_FLAGS.accgyrotasks = 0;
            break;
        }
    }
}
