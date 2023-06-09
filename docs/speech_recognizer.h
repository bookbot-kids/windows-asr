struct Configuration {
    std::string modelDir; // model folder path
    int modelSampleRate; // default 16khz
    std::string recordingDir; //folder to save aac recording
};

class SpeechRecognizer {
public:
    // Constructor
    SpeechRecognizer(const Configuration& config) : configuration(config) {}

    // Start to listen audio
    virtual void listen() = 0;

    // Stop to listen audio
    virtual void stopListening() = 0;

    // Pause listen
    virtual void mute() = 0;

    // Resume listen
    virtual void unmute() = 0;

    // Setup microphone and ASR model
    virtual void initialize(std::string recordingId) = 0; // set recordingId property

    // Release all resources
    virtual void release() = 0;
    
    // Reset ASR speech
    virtual void resetSpeech() = 0;    

    // flush speech 
    virtual void flushSpeech(std::string speechText) = 0; // set speech text property

    // Add a callback to receive string value from ASR
    virtual void addListener(const std::function<void(const std::string&)>& listener) = 0;

    // Remove a callback ASR
    virtual void removeListener(const std::function<void(const std::string&)>& listener) = 0;

    // Clear all listeners
    virtual void removeAllListeners() = 0;

private:
    Configuration configuration;
    std::string speechText;
    std::string recordingId;
};