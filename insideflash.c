#include "insideflash.h"
#include "string.h"
#include <stdio.h>

#define FLASH_DEBUG 1
#if FLASH_DEBUG
#define FLASH_INFO( fmt, args... ) 	printf( fmt, ##args )//KEY_VALUE_INFO(fmt, ##args)
#else
#define FLASH_INFO( fmt, args... )
#endif

/*******************************flash operation******************************************************/

uint32_t flash_sector_address( int16_t index ){
    
    if( index > SECTOR_NUM ){
        FLASH_INFO( "Fan area index error\r\n" );
        while( true );
    }
    
    uint32_t realaddr = FLASH_BASE;
    
    #if defined M3
        realaddr += ( index ) * 2 * 1024;
    #else
        //stm32f407ve
        if( index > 0 ){
            realaddr += ( index > 3 ? 4 : index ) * 16 * 1024;
        }
        if( index > 4 ){
            realaddr += 1 * 64 * 1024;
        }
        if( index > 5 ){
            realaddr += ( index - 5 ) * 128 * 1024;
        }
    #endif
    
    return realaddr;
}

bool flash_leagle_sector_address( int32_t flashaddr ){
    for( uint16_t i = 0; i < SECTOR_NUM; i++ ){
        if( flashaddr == flash_sector_address( i ) ){
            return true;
        }
    }
    return false;
}

int16_t flash_sector_index( uint32_t flashaddr ){
    for( int16_t i = 0; i < SECTOR_NUM; i++ ){
        if( flashaddr == flash_sector_address( i ) ){
            return i;
        }
    }
    return -1;
}


bool flash_erase( int32_t flashaddr, uint32_t page ){
    
    if( ( ( flashaddr < FLASH_BASE || flashaddr > FLASH_END_ADDR ) && flash_leagle_sector_address( flashaddr ) ) || page == 0){
        FLASH_INFO("flash_erase: erase para error\r\n");
        return false;
    }

    FLASH_EraseInitTypeDef f;
    
    #if defined M3
        f.TypeErase = FLASH_TYPEERASE_PAGES;
        
        f.PageAddress = flashaddr;
        
        f.NbPages = page;
	#else
        f.TypeErase = FLASH_TYPEERASE_SECTORS;
        
        f.Sector = flash_sector_index( flashaddr ) ;
        
        f.NbSectors = page;
        
        f.VoltageRange = FLASH_VOLTAGE_RANGE_3;
        
        f.Banks = FLASH_BANK_1;
    #endif
    
    uint8_t cycleCount = 0;

    uint32_t PageError = 0;
    
reerase:
	
    __disable_irq();
    HAL_FLASH_Unlock();
    
    #if defined M3
        #if !defined _STM32L_
            __HAL_FLASH_CLEAR_FLAG( FLASH_FLAG_BSY | FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPERR );
        #else
            __HAL_FLASH_CLEAR_FLAG( FLASH_FLAG_BSY | FLASH_FLAG_EOP | FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR | FLASH_FLAG_SIZERR | FLASH_FLAG_OPTVERR );
        #endif
    #else
        __HAL_FLASH_CLEAR_FLAG( FLASH_FLAG_BSY | FLASH_FLAG_EOP | FLASH_FLAG_PGSERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGAERR | FLASH_FLAG_WRPERR );
    #endif
    
	HAL_FLASHEx_Erase(&f, &PageError);
    
    HAL_FLASH_Lock();
    __enable_irq();
	
    if( PageError != 0xFFFFFFFF ){
        cycleCount ++;
        if( cycleCount >= 5 ){
            FLASH_INFO("erase failed\r\n");
            return false;
        }
        HAL_Delay(100);
        goto reerase;
    }
    
//    uint8_t *eraseaddr = (uint8_t *)flashaddr;
//    for( uint32_t i = 0; i < page * SECTOR_SIZE ; i ++ ){
//        if( eraseaddr[ i ] != (uint8_t) ERASURE_STATE ){
//            return false;
//        }
//    }
    
    return true;
}

bool flash_write( const uint8_t *ramaddr, uint32_t flashaddr, int32_t size ){
    
    if( ( flashaddr < FLASH_BASE || flashaddr > FLASH_END_ADDR ) || size == 0){
        FLASH_INFO("flash_write: para error\r\n");
        return false;
    }
    
    if( ( ( flashaddr - FLASH_BASE ) % 4) != 0 ){
		FLASH_INFO("flash_write: Writing a int address is wrong\r\n");
        return false;
    }
	
	if( size > SECTOR_SIZE_MIN ){
		FLASH_INFO("The amount of data is too large\r\n");
		return false;
	}
	
    uint32_t currentflashAddr   = flashaddr;
    const uint8_t  *currentram  = ramaddr;
    uint8_t  Remainder          = size % 4;
    uint16_t Multiple           = size / 4;
	
    __disable_irq();
    HAL_FLASH_Unlock();
    
    #if defined M3
        #if !defined _STM32L_
            __HAL_FLASH_CLEAR_FLAG( FLASH_FLAG_BSY | FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPERR );
        #else
            __HAL_FLASH_CLEAR_FLAG( FLASH_FLAG_BSY | FLASH_FLAG_EOP | FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR | FLASH_FLAG_SIZERR | FLASH_FLAG_OPTVERR );
        #endif
    #else
        __HAL_FLASH_CLEAR_FLAG( FLASH_FLAG_BSY | FLASH_FLAG_EOP | FLASH_FLAG_PGSERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGAERR | FLASH_FLAG_WRPERR );
    #endif
    
    for( int32_t i = 0; i < Multiple ; i ++  ){
        HAL_FLASH_Program( FLASH_TYPEPROGRAM_WORD, currentflashAddr, *((uint32_t *)currentram) );
        currentflashAddr += 4;
        currentram += 4;
    }
    
    if( Remainder ){
        uint8_t data[4] ;
		memset( data, 0x00, 4);//change
        for( uint8_t i = 0; i < Remainder; i++ ){
            data[i] = currentram[i];
        }
        HAL_FLASH_Program( FLASH_TYPEPROGRAM_WORD, currentflashAddr,  *((uint32_t *)data) );
    }
    
    HAL_FLASH_Lock();
	__enable_irq();
    
    if( memcmp( ramaddr, (uint8_t *)flashaddr, size) == 0 ){
        return true;
    }else{
        FLASH_INFO("inside flash write failure\r\n");
        return false;
    }
}

bool flash_read( int32_t flashaddr , uint8_t *ramBuffer , uint16_t bytesLen ){
    if( ( flashaddr < FLASH_BASE || flashaddr > FLASH_END_ADDR ) ||  bytesLen == 0){
        FLASH_INFO("flash_read :ROM address is wrong\r\n");
        return false;
    }
	while( bytesLen -- ){
	 *( ramBuffer ++ ) = *(( uint8_t* ) flashaddr ++ );
	}
    return true;
}


