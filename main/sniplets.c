            float ph_setpoint = -1, ph_current = -1;
            int acl_setpoint = -1, acl_current = -1;
            if( xSemaphoreTake( pAquaVal->lock, ( TickType_t ) 10 ) == pdTRUE ) {
              ph_setpoint  = pAquaVal->ph_setpoint;
              acl_setpoint = pAquaVal->acl_setpoint;
              ph_current   = pAquaVal->ph_current;
              acl_current  = pAquaVal->acl_current;
              xSemaphoreGive( pAquaVal->lock );
            }
            ESP_LOGD(TAG, "pH setpoint/current: %3.1f/%3.1f\n",ph_setpoint,ph_current);
            ESP_LOGD(TAG, "acl setpoint/current: %3d/%3d\n",acl_setpoint,acl_current);

