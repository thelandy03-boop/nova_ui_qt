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
        "desc: ACE EQ (Parametric 4-Band)\n" +
        "slider1:300<20,2000,1>Frequency 1 (Hz)\n" +
        "slider2:0<-18,18,0.1>Gain 1 (dB)\n" +
        "slider3:1.0<0.1,5,0.05>Bandwidth 1 (oct)\n" +
        "slider4:1000<100,10000,1>Frequency 2 (Hz)\n" +
        "slider5:0<-18,18,0.1>Gain 2 (dB)\n" +
        "slider6:1.0<0.1,5,0.05>Bandwidth 2 (oct)\n" +
        "slider7:3000<500,15000,1>Frequency 3 (Hz)\n" +
        "slider8:0<-18,18,0.1>Gain 3 (dB)\n" +
        "slider9:1.0<0.1,5,0.05>Bandwidth 3 (oct)\n" +
        "slider10:8000<1000,20000,1>Frequency 4 (Hz)\n" +
        "slider11:0<-18,18,0.1>Gain 4 (dB)\n" +
        "slider12:1.0<0.1,5,0.05>Bandwidth 4 (oct)\n\n" +
        "@init\n" +
        "pi = 3.141592653589793;\n" +
        "x1_1L = 0; x2_1L = 0; y1_1L = 0; y2_1L = 0;\n" +
        "x1_1R = 0; x2_1R = 0; y1_1R = 0; y2_1R = 0;\n" +
        "x1_2L = 0; x2_2L = 0; y1_2L = 0; y2_2L = 0;\n" +
        "x1_2R = 0; x2_2R = 0; y1_2R = 0; y2_2R = 0;\n" +
        "x1_3L = 0; x2_3L = 0; y1_3L = 0; y2_3L = 0;\n" +
        "x1_3R = 0; x2_3R = 0; y1_3R = 0; y2_3R = 0;\n" +
        "x1_4L = 0; x2_4L = 0; y1_4L = 0; y2_4L = 0;\n" +
        "x1_4R = 0; x2_4R = 0; y1_4R = 0; y2_4R = 0;\n\n" +
        "@slider\n" +
        "A1 = 10^(slider2 / 40); w1 = 2 * pi * slider1 / srate;\n" +
        "cos1 = cos(w1); sin1 = sin(w1); alpha1 = sin1 * slider3 * 0.5;\n" +
        "b0_1 = 1 + alpha1 * A1; b1_1 = -2 * cos1; b2_1 = 1 - alpha1 * A1;\n" +
        "a0_1 = 1 + alpha1 / A1; a1_1 = -2 * cos1; a2_1 = 1 - alpha1 / A1;\n\n" +
        "A2 = 10^(slider5 / 40); w2 = 2 * pi * slider4 / srate;\n" +
        "cos2 = cos(w2); sin2 = sin(w2); alpha2 = sin2 * slider6 * 0.5;\n" +
        "b0_2 = 1 + alpha2 * A2; b1_2 = -2 * cos2; b2_2 = 1 - alpha2 * A2;\n" +
        "a0_2 = 1 + alpha2 / A2; a1_2 = -2 * cos2; a2_2 = 1 - alpha2 / A2;\n\n" +
        "A3 = 10^(slider8 / 40); w3 = 2 * pi * slider7 / srate;\n" +
        "cos3 = cos(w3); sin3 = sin(w3); alpha3 = sin3 * slider9 * 0.5;\n" +
        "b0_3 = 1 + alpha3 * A3; b1_3 = -2 * cos3; b2_3 = 1 - alpha3 * A3;\n" +
        "a0_3 = 1 + alpha3 / A3; a1_3 = -2 * cos3; a2_3 = 1 - alpha3 / A3;\n\n" +
        "A4 = 10^(slider11 / 40); w4 = 2 * pi * slider10 / srate;\n" +
        "cos4 = cos(w4); sin4 = sin(w4); alpha4 = sin4 * slider12 * 0.5;\n" +
        "b0_4 = 1 + alpha4 * A4; b1_4 = -2 * cos4; b2_4 = 1 - alpha4 * A4;\n" +
        "a0_4 = 1 + alpha4 / A4; a1_4 = -2 * cos4; a2_4 = 1 - alpha4 / A4;\n\n" +
        "@sample\n" +
        "s1L = (b0_1 * spl0 + b1_1 * x1_1L + b2_1 * x2_1L - a1_1 * y1_1L - a2_1 * y2_1L) / a0_1;\n" +
        "x2_1L = x1_1L; x1_1L = spl0; y2_1L = y1_1L; y1_1L = s1L;\n" +
        "s2L = (b0_2 * s1L + b1_2 * x1_2L + b2_2 * x2_2L - a1_2 * y1_2L - a2_2 * y2_2L) / a0_2;\n" +
        "x2_2L = x1_2L; x1_2L = s1L; y2_2L = y1_2L; y2_2L = s2L;\n" +
        "s3L = (b0_3 * s2L + b1_3 * x1_3L + b2_3 * x2_3L - a1_3 * y1_3L - a2_3 * y2_3L) / a0_3;\n" +
        "x2_3L = x1_3L; x1_3L = s2L; y2_3L = y1_3L; y1_3L = s3L;\n" +
        "spl0 = (b0_4 * s3L + b1_4 * x1_4L + b2_4 * x2_4L - a1_4 * y1_4L - a2_4 * y2_4L) / a0_4;\n" +
        "x2_4L = x1_4L; x1_4L = s3L; y2_4L = y1_4L; y1_4L = spl0;\n\n" +
        "s1R = (b0_1 * spl1 + b1_1 * x1_1R + b2_1 * x2_1R - a1_1 * y1_1R - a2_1 * y2_1R) / a0_1;\n" +
        "x2_1R = x1_1R; x1_1R = spl1; y2_1R = y1_1R; y1_1R = s1R;\n" +
        "s2R = (b0_2 * s1R + b1_2 * x1_2R + b2_2 * x2_2R - a1_2 * y1_2R - a2_2 * y2_2R) / a0_2;\n" +
        "x2_2R = x1_2R; x1_2R = s1R; y2_2R = y1_2R; y2_2R = s2R;\n" +
        "s3R = (b0_3 * s2R + b1_3 * x1_3R + b2_3 * x2_3R - a1_3 * y1_3R - a2_3 * y2_3R) / a0_3;\n" +
        "x2_3R = x1_3R; x1_3R = s2R; y2_3R = y1_3R; y1_3R = s3R;\n" +
        "spl1 = (b0_4 * s3R + b1_4 * x1_4R + b2_4 * x2_4R - a1_4 * y1_4R - a2_4 * y2_4R) / a0_4;\n" +
        "x2_4R = x1_4R; x1_4R = s3R; y2_4R = y1_4R; y1_4R = spl1;\n"

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
