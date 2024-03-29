#include "dev/ata.h"

/*
 * Registers' cache for writing
 */
w_byte  reg_sect_count;
w_byte  reg_sect_number;
w_byte  reg_cyl_low;
w_byte  reg_cyl_high;
w_byte  reg_drive;

/*
 * Is init registers for writing
 */
w_byte  is_reg_init;

/*
 * Counters for IRQ 14,15
 */
w_dword irq14,irq15;   

IRQ_MASTER_HANDLER(irq_hdd){
    printf("IRQ 14: interrupt");
    irq14++;   
}

IRQ_MASTER_HANDLER(irq_hdd2){
    printf("IRQ 15: interrupt");
    irq15++;   
}

void* get_irq_hdd(){
    return &irq_hdd;
}

void* get_irq_hdd2(){
    return &irq_hdd2;
}

void wait_ns(w_dword cnt){
    w_dword idx;
    for(idx=0;idx<cnt*10;idx++){};
}

w_dword wait_status(w_word port,w_dword timeout,w_byte status,w_byte mode){
    
    w_dword time;
    w_byte  st;
    
    time = timeout;
    
    if(time == 0) return 0;
    
    while((((st = inportb(port+0x07))&status) != 0)^(mode)){
        time--;
        if(time == 0) return 0;            
    }    
    return time;
}

w_dword wait_non_busy(w_word port,w_dword timeout){
    return wait_status(port,timeout,STATUS_BUSY,MODE_NO);
}

w_dword wait_drq(w_word port,w_dword timeout){
    return wait_status(port,timeout,STATUS_DRQ,MODE_YES);
}

w_dword ata_detect(w_word port,w_byte sel){    
    w_byte  status;
    
    outportb(port+0x06,0xa0|sel);
    
    status = inportb(port+0x07);

    if(status&STATUS_ERR!=0){

        inportb(port+0x01);
        
       	DEBUG("*");
        
        status = inportb(port+0x07);

		DEBUG("*");

        if(status&&STATUS_ERR!=0){
            //can't corrected
            //TODO:
        }else{
            //Ok
        }
    }
    
    outportb(port+0x02,0x55);
    outportb(port+0x03,0xAA);
    
    if((inportb(port+0x02)!=0x55)||(inportb(port+0x03)!=0xAA)){
        printf("HDD: Port %x isn't detected\n",port);
        return ERROR_COMMON;
    } 
    
    return 0;  
}

void ata_init_regs(	w_byte  r_drive,
						w_byte  r_sect_count, 
						w_byte  r_sect_number,
						w_byte  r_cyl_low,
						w_byte  r_cyl_high
						)
{
	reg_drive = r_drive;
	reg_sect_count = r_sect_count;
	reg_sect_number = r_sect_number;
	reg_cyl_low = r_cyl_low;
	reg_cyl_high = r_cyl_high;
	is_reg_init = 1;
}

w_dword ata_cmd(w_byte cmd,w_word port,w_byte sel){ 
    if(is_reg_init==0) return ERROR_COMMON;   
    				 
    outportb(port+0x06,reg_drive|sel);
    outportb(port+0x02,reg_sect_count);   
    outportb(port+0x03,reg_sect_number);      
    outportb(port+0x04,reg_cyl_low);    
    outportb(port+0x05,reg_cyl_high); 
    outportb(port+0x07,cmd);  
    is_reg_init = 0;
}

w_dword ata_worker(ata_job_struct* job){
	
	w_dword time;
		
	time = ATA_TOTAL_TIME;
	
	/*
	 * Wait free bus
	 */
	 
	if((time = wait_non_busy(job->port,time)) == 0){
    	DEBUG("WAIT_NON_BUSY: error \n");
    	return ERROR_ATA;
    }
    
    /*
     * Set DEV bit to D/H
     */
	
	outportb(job->port+0x06,0xa0|job->device_id);      

    /*
     * Wait Drive Ready 
     */
    
    if((time = wait_drq(job->port,time)) == 0){
    	DEBUG("WAIT_DRQ: error\n");
    	return ERROR_ATA;
    }  
    
    outportb(job->port+0x02,job->sect_count);   
    outportb(job->port+0x03,job->sect_number);      
    outportb(job->port+0x04,job->cyl_low);    
    outportb(job->port+0x05,job->cyl_high); 
    outportb(job->port+0x07,job->command);  
    
    /*
     * TODO: Make job
     */  
           
    time = ATA_TOTAL_TIME - time;
    
    DEBUG("Worked time: %s", time);        
}
	
	
w_dword ata_read(w_dword port,void* hdd_buf,w_dword size){
    w_dword index;
    w_byte  status;
    w_word  temp;
    
    index = 0;
    status = inportb(port+0x07);

	DEBUG("Reading data...");

    while((status & STATUS_DRQ)!=0){
        temp = inportw(port);
        *(((w_word*)hdd_buf)+index) = temp;
        index++;

        while((status = inportb(port+0x07))&STATUS_BUSY!=0){
        	//TODO: error handling
        }  
        status = inportb(port+0x07);     
        if(index*2>size) return 0x1;
    }
    
    DEBUG("Ok\n");   
    
    return 0;
}

w_dword ata_read_data(w_word port, w_byte sel, w_byte  r_sect_number, w_byte  r_cyl_low, w_byte  r_cyl_high ){

    w_dword time;
    
    hdd_buf  = (void*)ATA_BUFFER;
	time = 500000;
	
    if((time = wait_non_busy(port,time)) == 0){
    	DEBUG("WAIT_NON_BUSY: error \n");
    	return ERROR_ATA;
    }
    
    ata_init_regs(0xa0,0x1,0x1,0x0,0x0);    
    
    if(ata_cmd(ATA_READ,port,sel)==ERROR_COMMON){
    	DEBUG("ATA_CMD: error\n");
    }
    
    if((time = wait_drq(port,time)) == 0){
    	DEBUG("WAIT_DRQ: error\n");
    	return ERROR_ATA;
    }    
    
    ata_read(port,hdd_buf,ATA_BUFFER_SIZE);			
}

w_dword ata_info(w_word port,w_byte sel){
    
    w_byte  status;
    w_word  tmp;
    w_dword time;
    w_dword index;
    w_byte  vnd[41];
    
    w_word  cyl;
    w_word  hds;
    w_word  sct;
    
	/*
	 * hdd_buffer for reading data
	 * Address: 0xAE00
	 * Size: 	0x200
	 */    

    hdd_buf  = (void*)ATA_BUFFER;
    
    time = 500000;
    
    time = wait_non_busy(port,time);
    if(time == 0){
    	DEBUG("WAIT_NON_BUSY: error \n");
    	return ERROR_ATA;
    }
    
    ata_init_regs(0xa0,0x1,0x1,0x0,0x0);    
    
    if(ata_cmd(ATA_INFO,port,sel)==ERROR_COMMON){
    	DEBUG("ATA_CMD: error\n");
    }
    
    time = wait_drq(port,time);    
    if(time == 0){
    	DEBUG("WAIT_DRQ: error\n");
    	return ERROR_ATA;
    }    
            
    ata_read(port,hdd_buf,ATA_BUFFER_SIZE);
    
    for(index=0;index<20;index++){
        tmp=*(((w_word*)hdd_buf)+index+27);
        *(vnd+index*2+1)=tmp&0x00FF;
        *(vnd+index*2)=(tmp&0xFF00)>>8;
    }
    
    cyl = *((w_word*)(hdd_buf)+1);
    hds = *((w_word*)(hdd_buf)+3);
    sct = *((w_word*)(hdd_buf)+6);
    
    printf("Status Register:        %x \n",inportb(port+0x07));
    printf("General configuration:  %x \n",*((w_word*)(hdd_buf)));
    printf("Vendor:                 %s \n",vnd);
    printf("Size:                   %i/%i/%i total= %i \n",cyl,hds,sct,cyl*hds*sct);
    printf("HDD_Test:               Complete \n");
    return 0;    
}

w_dword test_hdd(w_word port,w_byte sel){    

    w_byte  vnd[41];
    w_dword timeout;
    w_dword index;
    w_byte  status;
    w_word  temp;
    
    if(ata_detect(port,sel)!=0) return 0xFFFFFFFF;
    
    vnd[41]='\0';
    timeout = 5; 
    hdd_buf = (void*)0xAE00; 
    

    while(((status = inportb(port+0x07)) & STATUS_BUSY)!=0){
        if(timeout==0) return 0xFFFFFFFF;
        timeout--;
    }

	ata_init_regs(0xa0,0x1,0x1,0x0,0x0);
	ata_cmd(ATA_READ,port,sel);
	
/*    
    //Select primary channel
    outportb(0x1f6,0xa0);  
    
    outportb(0x1f2,0x1);   
    outportb(0x1f3,0x1);      
    outportb(0x1f4,0x0);    
    outportb(0x1f5,0x0); 
    
    //Run 'Identify Drive' command  
    outportb(0x1f7,0xEC); 
*/    
//    printf("Waiting hdd_buffers...\n");
    //Waiting hdd_buffers
    while((status = inportb(port+0x07))&STATUS_DRQ==0){
//        printf("Log: <><HDD><> Status = 0x%x Timeout=%i\n",status,timeout);
        if(timeout==0) return 0xFFFFFFFF;
        timeout--;
    };
    
    printf("Log: <><HDD><> Status = 0x%x Timeout=%i\n",status,timeout);
//    printf("Copying...\n");
    
    index = 0;
    printf("Log: <><HDD><> Status = 0x%x Result=0x%x\n",status,status & STATUS_DRQ);
    while((status & STATUS_DRQ)!=0){
        temp = inportw(port);
        *(((w_word*)hdd_buf)+index) = temp;
        index++;
        if(temp!=0){
            printf("*");
        }else{
            printf("0");
        }            
        while((status = inportb(port+0x07))&STATUS_BUSY!=0){
        	//TODO: Leak place
        }  
        status = inportb(port+0x07);     
    }    
    
    printf("\n");    
        
    //Copying
    //asm("movw $256,%cx\n movw _hdd_buf,%di\n movw $0x1f0,%dx\n rep insw");
    
/*    for(index=0;index<40;index++){
        vnd[index]=*((char*)hdd_buf+index+27*2);
    }
    
    printf("Status Register:        %x \n",inportb(0x1f7));
    printf("General configuration:  %x \n",*((w_word*)(hdd_buf)));
    printf("Cylinders:      %i \n",*((w_word*)(hdd_buf+2)));
    printf("ATA0 Vendor:    %s \n",vnd);
    printf("HDD_Test:       Complete \n");
*/
    status = inportb(port+0x07);
    printf("Final Status: 0x%x \n",status);
    printf("Must be LILO: 0x%x \n",*(w_dword*)(hdd_buf));
  
    return 0;
}         
