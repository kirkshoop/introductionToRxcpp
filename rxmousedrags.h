#pragma once

extern"C" void EMSCRIPTEN_KEEPALIVE rxmousedrags()
{
    auto down$ = mousedown$("#window").publish().connect_forever();
    auto up$ = mouseup$("#window").publish().connect_forever();
    auto move$ = mousemove$("#window").publish().connect_forever();

    lifetime.add(
        down$.
            map([=](MouseEvent){
                return move$.
                    take_until(up$).
                    map([](MouseEvent){return 1;}).
                    start_with(0).
                    sum();
            }).
            merge().
            map( 
                [](int c){
                    return to_string(c) + " moves while mouse down";
                }).
            subscribe( 
                println(cout),
                [](exception_ptr ep){cout << what(ep) << endl;}));

  EM_ASM(
    function sendEvent(type, data) {
      var event = document.createEvent('Event');
      event.initEvent(type, true, true);
      for(var d in data) event[d] = data[d];
      window.dispatchEvent(event);
    }
    sendEvent('mousemove', { screenX: 1, screenY: 1, clientX: 1, clientY: 1, button: 0, buttons: 0, 'movementX': 1, 'movementY': 1 });
    sendEvent('mousemove', { screenX: 1, screenY: 1, clientX: 1, clientY: 1, button: 0, buttons: 0, 'movementX': 1, 'movementY': 1 });
    sendEvent('mousedown', { screenX: 1, screenY: 1, clientX: 1, clientY: 1, button: 0, buttons: 1 });
    sendEvent('mousemove', { screenX: 1, screenY: 1, clientX: 1, clientY: 1, button: 0, buttons: 0, 'movementX': 1, 'movementY': 1 });
    sendEvent('mousemove', { screenX: 1, screenY: 1, clientX: 1, clientY: 1, button: 0, buttons: 0, 'movementX': 1, 'movementY': 1 });
    sendEvent('mouseup', { screenX: 1, screenY: 1, clientX: 1, clientY: 1, button: 0, buttons: 0 });
    sendEvent('mousemove', { screenX: 1, screenY: 1, clientX: 1, clientY: 1, button: 0, buttons: 0, 'movementX': 1, 'movementY': 1 });
  );
}
