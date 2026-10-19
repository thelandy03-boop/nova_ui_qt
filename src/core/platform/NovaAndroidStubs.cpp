#include "NovaAndroidStubs.h"

#if defined(Q_OS_ANDROID) || defined(Q_OS_WIN)

#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <QDateTime>

namespace PBD {
    Controllable::Controllable() : m_val(1.0) {}
    Controllable::~Controllable() = default;
    double Controllable::get_value() const { return m_val; }
    void Controllable::set_value(double val, GroupControlDisposition) { m_val = val; }
    
    ScopedConnectionList::ScopedConnectionList() = default;
    ScopedConnectionList::~ScopedConnectionList() = default;
    void ScopedConnectionList::drop_connections() {}
    
    PropertyList::PropertyList() = default;
    template<typename T, typename V>
    void PropertyList::add(T, V) {}
}

namespace Temporal {
    timepos_t::timepos_t(long long v) : m_val(v) {}
    long long timepos_t::samples() const { return m_val; }
    
    timecnt_t::timecnt_t(long long v) : m_val(v) {}
    long long timecnt_t::samples() const { return m_val; }
}

namespace ARDOUR {
    Sample = float;
    
    AutomationControl::AutomationControl() : Controllable() { m_val = 0.0; }
    AutomationControl::~AutomationControl() = default;
    
    GainControl::GainControl() : AutomationControl() { m_val = 1.0; }
    
    MuteControl::MuteControl() : AutomationControl() { m_val = 0.0; }
    bool MuteControl::muted() const { return m_val > 0.5; }
    
    SoloControl::SoloControl() : AutomationControl() { m_val = 0.0; }
    bool SoloControl::self_soloed() const { return m_val > 0.5; }
    
    namespace Properties {
        const char* start = "start";
        const char* length = "length";
        const char* name = "name";
        const char* layer = "layer";
        const char* whole_file = "whole_file";
        const char* opaque = "opaque";
    }
    
    BusProfile::BusProfile() : master_out_channels(2) {}
    
    Config::Config() = default;
    void Config::set_session_monitoring(MonitorChoice) {}
    
    Source::Source() = default;
    Source::~Source() = default;
    Temporal::timecnt_t Source::length() const { return Temporal::timecnt_t(0); }
    samplecnt_t Source::read(Sample*, samplepos_t, samplecnt_t, int) { return 0; }
    
    Region::Region() = default;
    Region::~Region() = default;
    ID Region::id() const { return ID(); }
    std::string Region::name() const { return "Region"; }
    samplepos_t Region::position_sample() const { return 0; }
    samplecnt_t Region::length_samples() const { return 0; }
    void Region::set_position(Temporal::timepos_t) {}
    void Region::set_length(Temporal::timecnt_t) {}
    
    AudioRegion::AudioRegion() = default;
    float AudioRegion::scale_amplitude() const { return 1.0f; }
    void AudioRegion::set_scale_amplitude(float) {}
    bool AudioRegion::fade_in_active() const { return false; }
    bool AudioRegion::fade_out_active() const { return false; }
    Temporal::timecnt_t AudioRegion::fade_in_length() const { return Temporal::timecnt_t(0); }
    Temporal::timecnt_t AudioRegion::fade_out_length() const { return Temporal::timecnt_t(0); }
    void AudioRegion::set_fade_in_active(bool) {}
    void AudioRegion::set_fade_out_active(bool) {}
    void AudioRegion::set_fade_in_length(samplecnt_t) {}
    void AudioRegion::set_fade_out_length(samplecnt_t) {}
    uint32_t AudioRegion::n_channels() const { return 2; }
    std::shared_ptr<Source> AudioRegion::audio_source(uint32_t) { return nullptr; }
    samplepos_t AudioRegion::start_sample() const { return 0; }
    
    Playlist::Playlist() = default;
    std::shared_ptr<RegionList> Playlist::region_list() { return std::make_shared<RegionList>(); }
    void Playlist::add_region(std::shared_ptr<Region>, Temporal::timepos_t) {}
    void Playlist::remove_region(std::shared_ptr<Region>) {}
    void Playlist::split_region(std::shared_ptr<Region>, Temporal::timepos_t const &) {}
    
    Route::Route(std::string name) : m_name(std::move(name)) {
        m_gain = std::make_shared<GainControl>();
        m_mute = std::make_shared<MuteControl>();
        m_solo = std::make_shared<SoloControl>();
        m_pan = std::make_shared<AutomationControl>();
        m_rec = std::make_shared<AutomationControl>();
    }
    Route::~Route() = default;
    bool Route::is_track() const { return false; }
    std::string Route::name() const { return m_name; }
    std::shared_ptr<GainControl> Route::gain_control() const { return m_gain; }
    std::shared_ptr<MuteControl> Route::mute_control() const { return m_mute; }
    std::shared_ptr<SoloControl> Route::solo_control() const { return m_solo; }
    std::shared_ptr<AutomationControl> Route::pan_azimuth_control() const { return m_pan; }
    
    Track::Track(std::string name) : Route(std::move(name)) {}
    bool Track::is_track() const override { return true; }
    std::shared_ptr<Playlist> Track::playlist() { return std::make_shared<Playlist>(); }
    std::shared_ptr<AutomationControl> Track::rec_enable_control() const { return m_rec; }
    
    AudioTrack::AudioTrack(std::string name) : Track(std::move(name)) {}
    std::shared_ptr<Source> AudioTrack::write_source(int) { return nullptr; }
    
    MidiTrack::MidiTrack(std::string name) : Track(std::move(name)) {}
    
    RouteGroup::RouteGroup() = default;
    
    AudioBackend::AudioBackend() = default;
    AudioBackend::~AudioBackend() = default;
    
    Session::Session(AudioEngine&, const std::string& path, const std::string& name, BusProfile*, const std::string&, bool)
        : m_path(path), m_name(name), m_routes(std::make_shared<RouteList>()) {}
    
    bool Session::dirty() const { return m_dirty; }
    void Session::set_dirty() { m_dirty = true; }
    int Session::save_state(const std::string&) { m_dirty = false; return 0; }
    
    std::string Session::path() const { return m_path; }
    std::string Session::name() const { return m_name; }
    double Session::sample_rate() const { return 48000.0; }
    std::shared_ptr<RouteList> Session::get_routes() { return m_routes; }
    void* Session::master_out() { return nullptr; }
    void Session::import_files(ImportStatus& s) { s.done = true; }
    
    AudioTrackList Session::new_audio_track(int, int, std::shared_ptr<RouteGroup>, uint32_t how_many,
                                           std::string name_template, PresentationInfo::order_t,
                                           TrackMode, bool, bool) {
        AudioTrackList list;
        for (uint32_t i = 0; i < how_many; ++i) {
            auto trk = std::make_shared<AudioTrack>(name_template);
            m_routes->push_back(trk);
            list.push_back(trk);
        }
        m_dirty = true;
        return list;
    }
    
    void Session::remove_route(std::shared_ptr<Route> r) {
        if (!r || !m_routes) return;
        auto it = std::find(m_routes->begin(), m_routes->end(), r);
        if (it != m_routes->end()) {
            m_routes->erase(it);
            m_dirty = true;
        }
    }
    
    AudioEngine* AudioEngine::create() { static AudioEngine eng; return &eng; }
    void AudioEngine::set_backend(const std::string&, const std::string&, const std::string&) {}
    void* AudioEngine::current_backend() { return this; }
    void AudioEngine::set_sample_rate(float) {}
    void AudioEngine::set_buffer_size(int) {}
    int AudioEngine::start() { return 0; }
    void AudioEngine::set_session(Session*) {}
    
    std::shared_ptr<Region> RegionFactory::create(const SourceList&, const PBD::PropertyList&) {
        return std::make_shared<AudioRegion>();
    }
}

#endif // Q_OS_ANDROID || Q_OS_WIN