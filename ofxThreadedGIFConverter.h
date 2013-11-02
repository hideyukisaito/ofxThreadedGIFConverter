#pragma once

#include "ofMain.h"
#include "ofxOpenGLContextScope.h"

struct ofxGIFConverterSetting
{
    ofxGIFConverterSetting() {}
    
    ofxGIFConverterSetting(int w, int h, float del, string dir, string name, bool save = true)
    {
        width = w;
        height = h;
        delay = del;
        outputDir = dir;
        filename = name;
        saveOriginal = save;
    }
    
    int width;
    int height;
    float delay;
    string outputDir;
    string filename;
    bool saveOriginal = true;
};

class ofxThreadedGIFConverter : public ofThread
{

    enum ofxGIFConverterType
    {
        CONVERTER_GM = 0, // Graphics Magick
        CONVERTER_IM = 1, // Image Magick
        CONVERTER_NOT_FOUND = 2,
    };

public:

    ~ofxThreadedGIFConverter()
    {
        if (isThreadRunning()) stopThread();
    }
    
    void setup(string binroot = "/usr/local/bin")
    {
        ofxOpenGLContextScope::setup();
        
        this->binroot = binroot;
        
        ofDirectory dir(this->binroot);
        dir.listDir();
        vector<ofFile> files = dir.getFiles();
        vector<string> filenames;
        
        for (int i = 0; i < files.size(); ++i)
        {
            filenames.push_back(files.at(i).getFileName());
        }
        
        vector<string>::iterator it = find(filenames.begin(), filenames.end(), "gm");
        if (it != filenames.end())
        {
            ofLogNotice("ofxThreadedGIFConverter") << "`gm` found";
            this->execPath = dir.getAbsolutePath() + "/gm convert";
            this->converterType = CONVERTER_GM;
        }
        else
        {
            it = find(filenames.begin(), filenames.end(), "convert");
            if (it != filenames.end())
            {
                ofLogNotice("ofxThreadedGIFConverter") << "`gm` not found, but `convert` found";
                this->execPath = dir.getAbsolutePath() + "/convert";
                this->converterType = CONVERTER_IM;
            }
            else
            {
                ofLogNotice("ofxThreadedGIFConverter") << "converter not found";
                this->converterType = CONVERTER_NOT_FOUND;
            }
        }
    }
    
    void add(ofxGIFConverterSetting setting, map<string, ofImage> images)
    {
        if (CONVERTER_NOT_FOUND == converterType)
        {
            ofLogError("ofxThreadedGIFConverter") << "GraphicsMagick or ImageMagick not found.";
            return;
        }
        
        lock();
        vector<ofImage> _images;
        for (map<string, ofImage>::iterator it = images.begin(); images.end() != it; ++it)
        {
            _images.push_back(ofImage((*it).second));
        }
        
        settings.push_back(setting);
        tasks.push_back(_images);
        unlock();
        
        if (!isThreadRunning()) startThread();
    }
    
    void add(ofxGIFConverterSetting setting, vector<ofImage> images)
    {
        lock();
        settings.push_back(setting);
        
        vector <ofImage> _images;
        for (int i = 0; i < images.size(); ++i)
        {
            _images.push_back(ofImage(images.at(0)));
        }
        tasks.push_back(images);
        unlock();
        
        if (!isThreadRunning()) startThread();
    }
    
    void setBinRoot(string path)
    {
        lock();
        this->binroot = path;
        unlock();
    }
    
    void threadedFunction()
    {
        while (isThreadRunning())
        {
            if (0 < tasks.size() && lock())
            {
                convert();
                unlock();
            }
            
            ofSleepMillis(10);
        }
    }
    
    ofxGIFConverterType getConverterType()
    {
        return converterType;
    }
    
    ofEvent<string> convertFinished;
    ofEvent<int> convertFailed;
    
private:

    ofxGIFConverterType converterType;
    string binroot;
    string execPath;
    
    vector<ofxGIFConverterSetting> settings;
    vector< vector<ofImage> > tasks;
    
    void convert()
    {
        ofxOpenGLContextScope scope;
        
        ofxGIFConverterSetting setting = settings.at(0);
        vector<ofImage> images = tasks.at(0);
        ofLogNotice("ofxThreadedGIFConverter") << "output dir : " + setting.outputDir;
        ofDirectory tempDir(setting.outputDir + "/temp");
        if (!tempDir.exists())
        {
            tempDir.create(true);
            ofLogNotice("ofxThreadedGIFConverter") << "create temp dir : " << tempDir.getAbsolutePath();
        }
        
        ofDirectory originalDir(setting.outputDir + "/original");
        if (setting.saveOriginal && !originalDir.exists())
        {
            originalDir.create(true);
            ofLogNotice("ofxThreadedGIFConverter") << "create original dir : " << originalDir.getAbsolutePath();
        }
        
        for (int i = 0; i < images.size(); ++i)
        {
            if (originalDir.exists())
            {
                images.at(i).saveImage(originalDir.getAbsolutePath() + "/" + ofToString(i) + ".png");
            }
            images.at(i).resize(setting.width, setting.height);
            images.at(i).saveImage(tempDir.getAbsolutePath() + "/" + ofToString(i) + ".png");
        }
        
        string convert_exec = execPath + " -delay " + ofToString(setting.delay) + " " + tempDir.getAbsolutePath() + "/*.png " + setting.outputDir + "/" + setting.filename;
        ofLogNotice("ofxThreadedGIFConverter") << "exec : " << convert_exec;
        int ret = system(convert_exec.c_str());
        
        if (0 == ret)
        {
            tempDir.remove(true);
            string result = setting.outputDir + "/" + setting.filename;
            ofNotifyEvent(convertFinished, result, this);
        }
        else
        {
            ofNotifyEvent(convertFailed, ret, this);
        }
        
        settings.erase(settings.begin());
        tasks.erase(tasks.begin());
        
        if (settings.empty() && tasks.empty()) stopThread();
    }
};