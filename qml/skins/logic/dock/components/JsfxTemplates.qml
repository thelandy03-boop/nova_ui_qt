import QtQuick

/**
 * Biblioteca modular de plantillas DSP para scripts JSFX de Cockos EEL2.
 * Contiene presets completos y funciones de formateo de scripts.
 */
QtObject {
    id: root

    function getTemplate(fname) {
        switch (fname) {
            case "simple_delay.jsfx":  return delayTemplate
            case "multi_tap.jsfx":     return multiTapTemplate
            case "chorus.jsfx":        return chorusTemplate
            case "phaser_stereo.jsfx": return phaserTemplate
            case "distortion.jsfx":    return distortionTemplate
            case "lfo_filter.jsfx":    return lfoFilterTemplate
            case "reverb.jsfx":        return reverbTemplate
            case "utils.jsh":          return utilsTemplate
            default:                   return delayTemplate
        }
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

    readonly property string delayTemplate:
        "// Simple Stereo Delay\n" +
        "desc:Stereo Delay Effect\n" +
        "slider1:200<0,1000,10>Delay (ms)\n" +
        "slider2:0.5<0,1,0.01>Feedback\n\n" +
        "@init\n" +
        "delay_samples = srate * (slider1 / 1000);\n" +
        "buf_pos = 0;\n" +
        "buf_len = delay_samples;\n\n" +
        "@slider\n" +
        "delay_samples = srate * (slider1 / 1000);\n\n" +
        "@sample\n" +
        "dry_L = spl0;\n" +
        "dry_R = spl1;\n" +
        "delayed_L = buf_pos[0];\n" +
        "spl0 = dry_L + delayed_L * slider2;\n" +
        "buf_pos[0] = dry_L;\n" +
        "buf_pos += 1;\n" +
        "if (buf_pos >= buf_len) then buf_pos = 0;\n"

    readonly property string multiTapTemplate:
        "// Multi-Tap Delay\n" +
        "desc:Multi-Tap Stereo Delay\n" +
        "slider1:250<10,1000,10>Tap 1 (ms)\n" +
        "slider2:500<10,2000,10>Tap 2 (ms)\n" +
        "slider3:0.4<0,1,0.01>Tap Feedback\n\n" +
        "@init\n" +
        "buf_pos = 0;\n" +
        "max_len = srate * 2;\n\n" +
        "@slider\n" +
        "t1 = floor(slider1 * 0.001 * srate);\n" +
        "t2 = floor(slider2 * 0.001 * srate);\n\n" +
        "@sample\n" +
        "p1 = (buf_pos - t1 + max_len) % max_len;\n" +
        "p2 = (buf_pos - t2 + max_len) % max_len;\n" +
        "tap1 = p1[0];\n" +
        "tap2 = p2[0];\n" +
        "buf_pos[0] = spl0 + (tap1 + tap2) * 0.5 * slider3;\n" +
        "spl0 = spl0 + tap1 * 0.6;\n" +
        "spl1 = spl1 + tap2 * 0.6;\n" +
        "buf_pos = (buf_pos + 1) % max_len;\n"

    readonly property string chorusTemplate:
        "// Stereo Chorus\n" +
        "desc:Vintage Stereo Chorus\n" +
        "slider1:1.5<0.1,5,0.1>Rate (Hz)\n" +
        "slider2:15<1,40,1>Depth (ms)\n" +
        "slider3:50<0,100,1>Mix (%)\n\n" +
        "@init\n" +
        "lfo_phase = 0;\n" +
        "buf_pos = 0;\n" +
        "max_del = srate * 0.1;\n\n" +
        "@slider\n" +
        "rate_inc = 2 * 3.14159265 * slider1 / srate;\n" +
        "depth_samp = slider2 * 0.001 * srate;\n" +
        "wet = slider3 / 100;\n" +
        "dry = 1 - wet;\n\n" +
        "@sample\n" +
        "mod = sin(lfo_phase) * depth_samp;\n" +
        "read_p = (buf_pos - floor(mod + depth_samp) + max_del) % max_del;\n" +
        "del_sig = read_p[0];\n" +
        "buf_pos[0] = (spl0 + spl1) * 0.5;\n" +
        "buf_pos = (buf_pos + 1) % max_del;\n" +
        "lfo_phase += rate_inc;\n" +
        "spl0 = spl0 * dry + del_sig * wet;\n" +
        "spl1 = spl1 * dry + del_sig * wet;\n"

    readonly property string phaserTemplate:
        "// Stereo Phaser\n" +
        "desc:4-Pole Analog Phaser\n" +
        "slider1:0.5<0.05,4,0.05>LFO Speed (Hz)\n" +
        "slider2:60<0,95,1>Feedback (%)\n\n" +
        "@init\n" +
        "phase = 0;\n" +
        "a1 = 0; a2 = 0; a3 = 0; a4 = 0;\n\n" +
        "@slider\n" +
        "inc = 2 * 3.14159265 * slider1 / srate;\n" +
        "fb = slider2 / 100;\n\n" +
        "@sample\n" +
        "lfo = (sin(phase) + 1) * 0.5;\n" +
        "phase += inc;\n" +
        "in = spl0 + a4 * fb;\n" +
        "a1 = in * (1 - lfo);\n" +
        "a2 = a1 * (1 - lfo);\n" +
        "a3 = a2 * (1 - lfo);\n" +
        "a4 = a3 * (1 - lfo);\n" +
        "spl0 = spl0 * 0.5 + a4 * 0.5;\n" +
        "spl1 = spl1 * 0.5 + a4 * 0.5;\n"

    readonly property string distortionTemplate:
        "// Hard Distortion\n" +
        "desc:Hard Knee Clipper Distortion\n" +
        "slider1:12<0,36,0.1>Drive (dB)\n" +
        "slider2:0<-24,6,0.1>Output Trim (dB)\n\n" +
        "@init\n" +
        "in_gain = 1;\n" +
        "out_gain = 1;\n\n" +
        "@slider\n" +
        "in_gain = 10^(slider1 / 20);\n" +
        "out_gain = 10^(slider2 / 20);\n\n" +
        "@sample\n" +
        "x0 = spl0 * in_gain;\n" +
        "x1 = spl1 * in_gain;\n" +
        "spl0 = min(1.0, max(-1.0, x0)) * out_gain;\n" +
        "spl1 = min(1.0, max(-1.0, x1)) * out_gain;\n"

    readonly property string lfoFilterTemplate:
        "// LFO Auto-Wah Filter\n" +
        "desc:LFO Modulated Resonant Filter\n" +
        "slider1:2.0<0.1,10,0.1>Rate (Hz)\n" +
        "slider2:800<200,4000,10>Center Freq (Hz)\n" +
        "slider3:2.5<0.5,10,0.1>Resonance (Q)\n\n" +
        "@init\n" +
        "lfo_p = 0;\n" +
        "s1 = 0; s2 = 0;\n\n" +
        "@slider\n" +
        "lfo_inc = 2 * 3.14159265 * slider1 / srate;\n\n" +
        "@sample\n" +
        "mod_freq = slider2 * (1 + 0.6 * sin(lfo_p));\n" +
        "lfo_p += lfo_inc;\n" +
        "w0 = 2 * 3.14159265 * mod_freq / srate;\n" +
        "alpha = sin(w0) / (2 * slider3);\n" +
        "b0 = (1 - cos(w0)) * 0.5;\n" +
        "b1 = 1 - cos(w0);\n" +
        "b2 = b0;\n" +
        "a0 = 1 + alpha;\n" +
        "a1 = -2 * cos(w0);\n" +
        "a2 = 1 - alpha;\n" +
        "out0 = (b0/a0)*spl0 + s1;\n" +
        "s1 = (b1/a0)*spl0 - (a1/a0)*out0 + s2;\n" +
        "s2 = (b2/a0)*spl0 - (a2/a0)*out0;\n" +
        "spl0 = out0;\n" +
        "spl1 = out0;\n"

    readonly property string reverbTemplate:
        "// Algorithmic Reverb\n" +
        "desc:4-Comb Stereo Reverb\n" +
        "slider1:45<0,100,1>Room Size (%)\n" +
        "slider2:30<0,100,1>Wet Mix (%)\n\n" +
        "@init\n" +
        "pos1 = 0; pos2 = 0; pos3 = 0;\n" +
        "len1 = 1557; len2 = 1617; len3 = 1491;\n\n" +
        "@slider\n" +
        "feedback = 0.5 + 0.45 * (slider1 / 100);\n" +
        "wet = slider2 / 100;\n" +
        "dry = 1 - wet;\n\n" +
        "@sample\n" +
        "sig = (spl0 + spl1) * 0.5;\n" +
        "out1 = pos1[0];\n" +
        "pos1[0] = sig + out1 * feedback;\n" +
        "pos1 = (pos1 + 1) % len1;\n" +
        "out2 = (pos2 + 2000)[0];\n" +
        "(pos2 + 2000)[0] = sig + out2 * feedback;\n" +
        "pos2 = (pos2 + 1) % len2;\n" +
        "rev_sig = (out1 + out2) * 0.5;\n" +
        "spl0 = spl0 * dry + rev_sig * wet;\n" +
        "spl1 = spl1 * dry + rev_sig * wet;\n"

    readonly property string utilsTemplate:
        "// Utility Functions Header\n" +
        "desc:NOVA DSP Math Utilities (Header)\n\n" +
        "function db2gain(db) (\n" +
        "    10 ^ (db / 20);\n" +
        ");\n\n" +
        "function gain2db(gain) (\n" +
        "    gain <= 0.00001 ? -100 : 20 * log10(gain);\n" +
        ");\n\n" +
        "function clip(val, min_v, max_v) (\n" +
        "    val < min_v ? min_v : (val > max_v ? max_v : val);\n" +
        ");\n"
}
