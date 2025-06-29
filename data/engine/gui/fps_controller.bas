renderFps = gui.getinvalue("FPS", "renderFps")
realRenderDt = gui.getinvalue("FPS", "realRenderDt")
gui.setvalue("FPS", "fpsText", "FPS: " + xl.realtostr(renderFps, 2) + ", DT: " + xl.realtostr(1.0 / renderFps, 6) + ", RDT: " + xl.realtostr(realRenderDt, 6) + ", " + xl.realtostr((realRenderDt / (1.0 / renderFps)) * 100, 2) + "% of budget")
