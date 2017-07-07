// board enforces
//   in-state
//      accel set/resume
//   out-state
//      cancel button, pedals, regen paddle


// all commands: brake and steering
// if controls_allowed
//     allow all commands up to limit
// else
//     block all commands that produce actuation

void safety_rx_hook(CAN_FIFOMailBox_TypeDef *to_push) {
  // state machine to enter and exit controls
  uint32_t addr;
  if (to_push->RIR & 4) {
    // Extended. Drop id of sender ECU
    addr = (to_push->RIR >> 3) & 0x1ffff000;
  } else {
    // Normal
    addr = to_push->RIR >> 21;
  }

  // Chevy Volt gen 2
  if (addr == 0x10758000) {
    int buttons = to_push->RDLR & 0xe;
    // res/set - enable, cancel button - disable
    if (buttons == 4 || buttons == 6) {
      controls_allowed = 1;
    } else if (buttons == 0xc) {
      controls_allowed = 0;
    }
  }

  #if 0 // TODOS
  // TODO: exit controls on brake press
  if (addr == 241) {
    // Brake pedal's potentiometer returns near-zero reading
    // even when pedal is not pressed
    if ( > 5) {
      controls_allowed = 0;
    }
  }

  // TODO: exit controls on gas press
  if (addr == 190) {
    if (gas > 0) {
      controls_allowed = 0;
    }
  }

  // TODO: exit controls on regen paddle
  if (addr == 189) {
    if (regen) {
      controls_allowed = 0;
    }
  }
  #endif
}

int safety_tx_hook(CAN_FIFOMailBox_TypeDef *to_send, int hardwired) {
  uint32_t addr;
  if (to_send->RIR & 4) {
    // Extended. Drop id of sender ECU
    addr = (to_send->RIR >> 3) & 0x1ffff000;
  } else {
    // Normal
    addr = to_send->RIR >> 21;
  }

  #if 0 // TODOs
  // BRAKE: safety check
  if (addr == ) {
    if (controls_allowed) {
      to_send->RDLR &= 0xFFFFFF3F;
    } else {
      to_send->RDLR &= 0xFFFF0000;
    }
  }

  // STEER: safety check
  if (addr == ) {
    if (controls_allowed) {
      to_send->RDLR &= 0xFFFFFFFF;
    } else {
      to_send->RDLR &= 0xFFFF0000;
    }
  }

  // GAS: safety check
  if (addr == ) {
    if (controls_allowed) {
      to_send->RDLR &= 0xFFFFFFFF;
    } else {
      to_send->RDLR &= 0xFFFF0000;
    }
  }
  #endif

  // 1 allows the message through
  return hardwired;
}

int safety_tx_lin_hook(int lin_num, uint8_t *data, int len, int hardwired) {
  return hardwired;
}

