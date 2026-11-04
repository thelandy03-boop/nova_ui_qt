import QtQuick

/**
 * Biblioteca modular de plantillas DSP para scripts JSFX de Cockos EEL2.
 * Contiene el código real de procesamiento para todos los plugins nativos del sistema.
 */
QtObject {
    id: root

    function getTemplate(fname) {
        var lower = (fname || "").toLowerCase()
        if (lower.indexOf("eq") !== -1) return eqTemplate
        if (lower.indexOf("comp") !== -1) return compTemplate
        if (lower.indexOf("gate") !== -1) return gateTemplate
        if (lower.indexOf("delay") !== -1) return delayTemplate
        if (lower.indexOf("reverb") !== -1) return reverbTemplate
        if (lower.indexOf("tube") !== -1 || lower.indexOf("saturat") !== -1) return tubeTemplate
        if (lower.indexOf("widen") !== -1 || lower.indexOf("stereo") !== -1) return widenerTemplate
        if (lower.indexOf("air") !== -1 || lower.indexOf("excit") !== -1) return vocalAirTemplate
        return eqTemplate
    }

    function formatScript(rawCode) {
        if (!rawCode) return ""
        var lines = rawCode.split("\n")
        var out = []
        for (var i = 0; i < lines.length; ++i) {
            var l = lines[i].trim()
            if (l.indexOf("@") === 0) {
                out.push("")
                out.push(l)
            } else if (l.length > 0) {
                out.push(l)
            }
        }
        return out.join("\n").trim() + "\n"
    }

    // 1. ACE EQ
    readonly property string eqTemplate:
        "desc: ACE EQ (Parametric 3-Band)\n" +
        "slider1:0<-12,12,0.1>Graves 80Hz (dB)\n" +
        "slider2:0<-12,12,0.1>Medios 1kHz (dB)\n" +
        "slider3:0<-12,12,0.1>Agudos 10kHz (dB)\n\n" +
        "@init\n" +
        "pi = 3.141592653589793;\n" +
        "x1_lL = 0; x2_lL = 0; y1_lL = 0; y2_lL = 0;\n" +
        "x1_lR = 0; x2_lR = 0; y1_lR = 0; y2_lR = 0;\n" +
        "x1_mL = 0; x2_mL = 0; y1_mL = 0; y2_mL = 0;\n" +
        "x1_mR = 0; x2_mR = 0; y1_mR = 0; y2_mR = 0;\n" +
        "x1_hL = 0; x2_hL = 0; y1_hL = 0; y2_hL = 0;\n" +
        "x1_hR = 0; x2_hR = 0; y1_hR = 0; y2_hR = 0;\n\n" +
        "@slider\n" +
        "A_l = 10^(slider1 / 40);\n" +
        "w0_l = 2 * pi * 80 / srate;\n" +
        "cos_l = cos(w0_l); sin_l = sin(w0_l);\n" +
        "alpha_l = sin_l / (2 * 0.7071);\n" +
        "b0_l = A_l * ((A_l + 1) - (A_l - 1) * cos_l + 2 * sqrt(A_l) * alpha_l);\n" +
        "b1_l = 2 * A_l * ((A_l - 1) - (A_l + 1) * cos_l);\n" +
        "b2_l = A_l * ((A_l + 1) - (A_l - 1) * cos_l - 2 * sqrt(A_l) * alpha_l);\n" +
        "a0_l = (A_l + 1) + (A_l - 1) * cos_l + 2 * sqrt(A_l) * alpha_l;\n" +
        "a1_l = -2 * ((A_l - 1) + (A_l + 1) * cos_l);\n" +
        "a2_l = (A_l + 1) + (A_l - 1) * cos_l - 2 * sqrt(A_l) * alpha_l;\n\n" +
        "A_m = 10^(slider2 / 40);\n" +
        "w0_m = 2 * pi * 1000 / srate;\n" +
        "cos_m = cos(w0_m); sin_m = sin(w0_m);\n" +
        "alpha_m = sin_m / (2 * 1.0);\n" +
        "b0_m = 1 + alpha_m * A_m;\n" +
        "b1_m = -2 * cos_m;\n" +
        "b2_m = 1 - alpha_m * A_m;\n" +
        "a0_m = 1 + alpha_m / A_m;\n" +
        "a1_m = -2 * cos_m;\n" +
        "a2_m = 1 - alpha_m / A_m;\n\n" +
        "A_h = 10^(slider3 / 40);\n" +
        "w0_h = 2 * pi * 10000 / srate;\n" +
        "cos_h = cos(w0_h); sin_h = sin(w0_h);\n" +
        "alpha_h = sin_h / (2 * 0.7071);\n" +
        "b0_h = A_h * ((A_h + 1) + (A_h - 1) * cos_h + 2 * sqrt(A_h) * alpha_h);\n" +
        "b1_h = -2 * A_h * ((A_h - 1) + (A_h + 1) * cos_h);\n" +
        "b2_h = A_h * ((A_h + 1) - (A_h - 1) * cos_h - 2 * sqrt(A_h) * alpha_h);\n" +
        "a0_h = (A_h + 1) - (A_h - 1) * cos_h + 2 * sqrt(A_h) * alpha_h;\n" +
        "a1_h = 2 * ((A_h - 1) - (A_h + 1) * cos_h);\n" +
        "a2_h = (A_h + 1) - (A_h - 1) * cos_h - 2 * sqrt(A_h) * alpha_h;\n\n" +
        "@sample\n" +
        "outL = (b0_l * spl0 + b1_l * x1_lL + b2_l * x2_lL - a1_l * y1_lL - a2_l * y2_lL) / a0_l;\n" +
        "x2_lL = x1_lL; x1_lL = spl0; y2_lL = y1_lL; y1_lL = outL;\n" +
        "outL2 = (b0_m * outL + b1_m * x1_mL + b2_m * x2_mL - a1_m * y1_mL - a2_m * y2_mL) / a0_m;\n" +
        "x2_mL = x1_mL; x1_mL = outL; y2_mL = y1_mL; y1_mL = outL2;\n" +
        "spl0 = (b0_h * outL2 + b1_h * x1_hL + b2_h * x2_hL - a1_h * y1_hL - a2_h * y2_hL) / a0_h;\n" +
        "x2_hL = x1_hL; x1_hL = outL2; y2_hL = y1_hL; y1_hL = spl0;\n\n" +
        "outR = (b0_l * spl1 + b1_l * x1_lR + b2_l * x2_lR - a1_l * y1_lR - a2_l * y2_lR) / a0_l;\n" +
        "x2_lR = x1_lR; x1_lR = spl1; y2_lR = y1_lR; y1_lR = outR;\n" +
        "outR2 = (b0_m * outR + b1_m * x1_mR + b2_m * x2_mR - a1_m * y1_mR - a2_m * y2_mR) / a0_m;\n" +
        "x2_mR = x1_mR; x1_mR = outR; y2_mR = y1_mR; y1_mR = outR2;\n" +
        "spl1 = (b0_h * outR2 + b1_h * x1_hR + b2_h * x2_hR - a1_h * y1_hR - a2_h * y2_hR) / a0_h;\n" +
        "x2_hR = x1_hR; x1_hR = outR2; y2_hR = y1_hR; y1_hR = spl1;\n"

    // 2. ACE COMPRESSOR
    readonly property string compTemplate:
        "desc: ACE Compressor (Studio Dynamics)\n" +
        "slider1:-12<-40,0,0.5>Umbral (dB)\n" +
        "slider2:3<1,20,0.5>Ratio (:1)\n" +
        "slider3:2<0,20,0.5>Ganancia (dB)\n\n" +
        "@init\n" +
        "env = 0;\n" +
        "gain_red = 1;\n\n" +
        "@slider\n" +
        "thresh_db = slider1;\n" +
        "ratio = slider2;\n" +
        "makeup = 10^(slider3 / 20);\n" +
        "att = exp(-1 / (0.010 * srate));\n" +
        "rel = exp(-1 / (0.100 * srate));\n\n" +
        "@sample\n" +
        "max_s = max(abs(spl0), abs(spl1));\n" +
        "env = (max_s > env) ? (att * env + (1 - att) * max_s) : (rel * env + (1 - rel) * max_s);\n" +
        "env_db = 20 * log10(max(env, 0.00001));\n\n" +
        "excess = env_db - thresh_db;\n" +
        "gr_db = (excess > 0 && ratio > 1) ? (excess * (1 - 1 / ratio)) : 0;\n" +
        "target_gain = 10^(-gr_db / 20);\n" +
        "gain_red = 0.9 * gain_red + 0.1 * target_gain;\n" +
        "final_gain = gain_red * makeup;\n" +
        "spl0 = spl0 * final_gain;\n" +
        "spl1 = spl1 * final_gain;\n"

    // 3. FAST NOISE GATE
    readonly property string gateTemplate:
        "desc: Fast Noise Gate (JSFX)\n" +
        "slider1:-45<-80,0,1>Umbral (dB)\n" +
        "slider2:50<5,500,5>Relajación (ms)\n\n" +
        "@init\n" +
        "env = 0;\n\n" +
        "@slider\n" +
        "thresh = 10^(slider1 / 20);\n" +
        "rel = exp(-1 / (slider2 * 0.001 * srate));\n\n" +
        "@sample\n" +
        "lvl = max(abs(spl0), abs(spl1));\n" +
        "env = (lvl > env) ? lvl : (env * rel);\n" +
        "gain = (env > thresh) ? 1.0 : (env / (thresh + 0.00001));\n" +
        "spl0 = spl0 * gain;\n" +
        "spl1 = spl1 * gain;\n"

    // 4. ACE DELAY
    readonly property string delayTemplate:
        "desc: ACE Delay (Stereo Tap)\n" +
        "slider1:250<10,1000,10>Tiempo (ms)\n" +
        "slider2:30<0,95,1>Feedback (%)\n" +
        "slider3:25<0,100,1>Mezcla (%)\n\n" +
        "@init\n" +
        "buf_pos = 0;\n" +
        "max_samples = 96000;\n\n" +
        "@slider\n" +
        "delay_samples = floor(slider1 * 0.001 * srate);\n" +
        "delay_samples = min(max_samples - 1, max(10, delay_samples));\n" +
        "fb = slider2 / 100;\n" +
        "wet = slider3 / 100;\n" +
        "dry = 1 - wet;\n\n" +
        "@sample\n" +
        "read_pos = (buf_pos - delay_samples + max_samples) % max_samples;\n" +
        "delayed_L = read_pos[0];\n" +
        "delayed_R = (read_pos + 100000)[0];\n\n" +
        "buf_pos[0] = spl0 + delayed_L * fb;\n" +
        "(buf_pos + 100000)[0] = spl1 + delayed_R * fb;\n\n" +
        "spl0 = spl0 * dry + delayed_L * wet;\n" +
        "spl1 = spl1 * dry + delayed_R * wet;\n\n" +
        "buf_pos = (buf_pos + 1) % max_samples;\n"

    // 5. ACE REVERB
    readonly property string reverbTemplate:
        "desc: ACE Reverb (Studio Room)\n" +
        "slider1:45<0,100,1>Tamaño Sala (%)\n" +
        "slider2:20<0,100,1>Mezcla Wet (%)\n\n" +
        "@init\n" +
        "pos1 = 0; pos2 = 0; pos3 = 0; pos4 = 0;\n" +
        "pos5 = 0; pos6 = 0;\n" +
        "del1 = 1116; del2 = 1188; del3 = 1277; del4 = 1356;\n" +
        "ap1 = 556; ap2 = 441;\n\n" +
        "@slider\n" +
        "room = 0.7 + (slider1 / 100) * 0.28;\n" +
        "wet = slider2 / 100;\n" +
        "dry = 1 - wet;\n\n" +
        "@sample\n" +
        "in_mono = (spl0 + spl1) * 0.015;\n\n" +
        "out1 = pos1[0];\n" +
        "pos1[0] = in_mono + out1 * room;\n" +
        "pos1 = (pos1 + 1) % del1;\n\n" +
        "out2 = (pos2 + 2000)[0];\n" +
        "(pos2 + 2000)[0] = in_mono + out2 * room;\n" +
        "pos2 = (pos2 + 1) % del2;\n\n" +
        "out3 = (pos3 + 4000)[0];\n" +
        "(pos3 + 4000)[0] = in_mono + out3 * room;\n" +
        "pos3 = (pos3 + 1) % del3;\n\n" +
        "out4 = (pos4 + 6000)[0];\n" +
        "(pos4 + 6000)[0] = in_mono + out4 * room;\n" +
        "pos4 = (pos4 + 1) % del4;\n\n" +
        "comb_sum = out1 + out2 + out3 + out4;\n\n" +
        "buf_ap1 = (pos5 + 8000)[0];\n" +
        "ap_out1 = -comb_sum + buf_ap1;\n" +
        "(pos5 + 8000)[0] = comb_sum + buf_ap1 * 0.5;\n" +
        "pos5 = (pos5 + 1) % ap1;\n\n" +
        "buf_ap2 = (pos6 + 10000)[0];\n" +
        "ap_out2 = -ap_out1 + buf_ap2;\n" +
        "(pos6 + 10000)[0] = ap_out1 + buf_ap2 * 0.5;\n" +
        "pos6 = (pos6 + 1) % ap2;\n\n" +
        "rev_sig = ap_out2 * 2.0;\n\n" +
        "spl0 = spl0 * dry + rev_sig * wet;\n" +
        "spl1 = spl1 * dry + rev_sig * wet;\n"

    // 6. ANALOG TUBE SATURATOR
    readonly property string tubeTemplate:
        "desc: Analog Tube Saturator (JSFX)\n" +
        "slider1:6<0,24,0.1>Drive (dB)\n" +
        "slider2:50<0,100,1>Calidez (%)\n" +
        "slider3:0<-24,12,0.1>Salida (dB)\n\n" +
        "@init\n" +
        "gain = 1;\n" +
        "warmth = 0.5;\n" +
        "out_gain = 1;\n\n" +
        "@slider\n" +
        "gain = 10^(slider1 / 20);\n" +
        "warmth = slider2 / 100;\n" +
        "out_gain = 10^(slider3 / 20);\n\n" +
        "@sample\n" +
        "x0 = spl0 * gain;\n" +
        "x1 = spl1 * gain;\n" +
        "spl0 = (tanh(x0) + warmth * (x0 * x0 * 0.15)) * out_gain;\n" +
        "spl1 = (tanh(x1) + warmth * (x1 * x1 * 0.15)) * out_gain;\n"

    // 7. STEREO WIDENER MID/SIDE
    readonly property string widenerTemplate:
        "desc: Stereo Widener Mid/Side (JSFX)\n" +
        "slider1:130<0,200,1>Ancho Estéreo (%)\n" +
        "slider2:0<-12,12,0.1>Nivel Mid (dB)\n\n" +
        "@init\n" +
        "width = 1.3;\n" +
        "mid_gain = 1.0;\n\n" +
        "@slider\n" +
        "width = slider1 / 100;\n" +
        "mid_gain = 10^(slider2 / 20);\n\n" +
        "@sample\n" +
        "mid = (spl0 + spl1) * 0.5 * mid_gain;\n" +
        "side = (spl0 - spl1) * 0.5 * width;\n" +
        "spl0 = mid + side;\n" +
        "spl1 = mid - side;\n"

    // 8. VOCAL AIR EXCITER
    readonly property string vocalAirTemplate:
        "desc: Vocal Air Exciter (JSFX)\n" +
        "slider1:35<0,100,1>Presencia Aérea (%)\n" +
        "slider2:10000<6000,16000,100>Corte (Hz)\n\n" +
        "@init\n" +
        "hpL = 0;\n" +
        "hpR = 0;\n\n" +
        "@slider\n" +
        "mix = slider1 / 100;\n" +
        "alpha = 0.5;\n\n" +
        "@sample\n" +
        "highL = spl0 - hpL;\n" +
        "highR = spl1 - hpR;\n" +
        "hpL = hpL + 0.3 * highL;\n" +
        "hpR = hpR + 0.3 * highR;\n" +
        "spl0 = spl0 + tanh(highL * 2.5) * mix * 0.4;\n" +
        "spl1 = spl1 + tanh(highR * 2.5) * mix * 0.4;\n"
}
