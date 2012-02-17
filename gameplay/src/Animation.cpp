#include "Base.h"
#include "Animation.h"
#include "AnimationController.h"
#include "AnimationClip.h"
#include "AnimationTarget.h"
#include "Game.h"
#include "Transform.h"
#include "Properties.h"

#define ANIMATION_INDEFINITE_STR "INDEFINITE"
#define ANIMATION_DEFAULT_CLIP 0
#define ANIMATION_ROTATE_OFFSET 0
#define ANIMATION_SRT_OFFSET 3

namespace gameplay
{

Animation::Animation(const char* id, AnimationTarget* target, int propertyId, unsigned int keyCount, unsigned long* keyTimes, float* keyValues, unsigned int type)
    : _controller(Game::getInstance()->getAnimationController()), _id(id), _duration(0), _defaultClip(NULL), _clips(NULL)
{
    createChannel(target, propertyId, keyCount, keyTimes, keyValues, type);
}

Animation::Animation(const char* id, AnimationTarget* target, int propertyId, unsigned int keyCount, unsigned long* keyTimes, float* keyValues, float* keyInValue, float* keyOutValue, unsigned int type)
    : _controller(Game::getInstance()->getAnimationController()), _id(id), _duration(0), _defaultClip(NULL), _clips(NULL)
{
    createChannel(target, propertyId, keyCount, keyTimes, keyValues, keyInValue, keyOutValue, type);
}

Animation::~Animation()
{
    if (_defaultClip)
    {
        if (_defaultClip->isClipStateBitSet(AnimationClip::CLIP_IS_PLAYING_BIT))
            _controller->unschedule(_defaultClip);
        SAFE_RELEASE(_defaultClip);
    }

    if (_clips)
    {
        std::vector<AnimationClip*>::iterator clipIter = _clips->begin();
    
        while (clipIter != _clips->end())
        {   
            AnimationClip* clip = *clipIter;
            if (clip->isClipStateBitSet(AnimationClip::CLIP_IS_PLAYING_BIT))
                _controller->unschedule(clip);
            SAFE_RELEASE(clip);
            clipIter++;
        }
        _clips->clear();
    }
    SAFE_DELETE(_clips);
}

Animation::Channel::Channel(Animation* animation, AnimationTarget* target, int propertyId, Curve* curve, unsigned long duration)
    : _animation(animation), _target(target), _propertyId(propertyId), _curve(curve), _duration(duration)
{
    // get property component count, and ensure the property exists on the AnimationTarget by getting the property component count.
    assert(_target->getAnimationPropertyComponentCount(propertyId));

    _animation->addRef();

    _target->addChannel(this);
}

Animation::Channel::~Channel()
{
    SAFE_DELETE(_curve);
    SAFE_RELEASE(_animation);
}

const char* Animation::getId() const
{
    return _id.c_str();
}

unsigned long Animation::getDuration() const
{
    return _duration;
}

void Animation::createClips(const char* animationFile)
{
    assert(animationFile);

    Properties* properties = Properties::create(animationFile);
    assert(properties);

    Properties* pAnimation = properties->getNextNamespace();
    assert(pAnimation);
    
    int frameCount = pAnimation->getInt("frameCount");
    assert(frameCount > 0);

    createClips(pAnimation, (unsigned int)frameCount);

    SAFE_DELETE(properties);
}

AnimationClip* Animation::createClip(const char* id, unsigned long start, unsigned long end)
{
    AnimationClip* clip = new AnimationClip(id, this, start, end);
    addClip(clip);
    return clip;
}

AnimationClip* Animation::getClip(const char* id)
{
    // If id is NULL return the default clip.
    if (id == NULL)
    {
        if (_defaultClip == NULL)
            createDefaultClip();

        return _defaultClip;
    }
    else
    {
        return findClip(id);
    }
}

void Animation::play(const char* clipId)
{
    // If id is NULL, play the default clip.
    if (clipId == NULL)
    {
        if (_defaultClip == NULL)
            createDefaultClip();
        
        _defaultClip->play();
    }
    else
    {
        // Find animation clip.. and play.
        AnimationClip* clip = findClip(clipId);
        if (clip != NULL)
            clip->play();
    }
}

void Animation::stop(const char* clipId)
{
    // If id is NULL, play the default clip.
    if (clipId == NULL)
    {
        if (_defaultClip)
            _defaultClip->stop();
    }
    else
    {
        // Find animation clip.. and play.
        AnimationClip* clip = findClip(clipId);
        if (clip != NULL)
            clip->stop();
    }
}

void Animation::pause(const char * clipId)
{
    if (clipId == NULL)
    {
        if (_defaultClip)
            _defaultClip->pause();
    }
    else
    {
        AnimationClip* clip = findClip(clipId);
        if (clip != NULL)
            clip->pause();
    }
}

void Animation::createDefaultClip()
{
    _defaultClip = new AnimationClip("default_clip", this, 0.0f, _duration);
}

void Animation::createClips(Properties* animationProperties, unsigned int frameCount)
{   
    assert(animationProperties);
    
    Properties* pClip = animationProperties->getNextNamespace();
    
    while (pClip != NULL && std::strcmp(pClip->getNamespace(), "clip") == 0)
    {
        int begin = pClip->getInt("begin");
        int end = pClip->getInt("end");

        AnimationClip* clip = createClip(pClip->getId(), ((float) begin / frameCount) * _duration, ((float) end / frameCount) * _duration);

        const char* repeat = pClip->getString("repeatCount");
        if (repeat)
        {
            if (strcmp(repeat, ANIMATION_INDEFINITE_STR) == 0)
            {
                clip->setRepeatCount(AnimationClip::REPEAT_INDEFINITE);
            }
            else
            {
                float value;
                sscanf(repeat, "%f", &value);
                clip->setRepeatCount(value);
            }
        }

        const char* speed = pClip->getString("speed");
        if (speed)
        {
            float value;
            sscanf(speed, "%f", &value);
            clip->setSpeed(value);
        }

        pClip = animationProperties->getNextNamespace();
    }
}

void Animation::addClip(AnimationClip* clip)
{
    if (_clips == NULL)
        _clips = new std::vector<AnimationClip*>;

    _clips->push_back(clip);
}

AnimationClip* Animation::findClip(const char* id) const
{
    if (_clips)
    {
        AnimationClip* clip = NULL;
        unsigned int clipCount = _clips->size();
        for (unsigned int i = 0; i < clipCount; i++)
        {
            clip = _clips->at(i);
            if (clip->_id.compare(id) == 0)
            {
                return _clips->at(i);
            }
        }
    }
    return NULL;
}

Animation::Channel* Animation::createChannel(AnimationTarget* target, int propertyId, unsigned int keyCount, unsigned long* keyTimes, float* keyValues, unsigned int type)
{
    unsigned int propertyComponentCount = target->getAnimationPropertyComponentCount(propertyId);
    assert(propertyComponentCount > 0);

    Curve* curve = new Curve(keyCount, propertyComponentCount);
    if (target->_targetType == AnimationTarget::TRANSFORM)
        setTransformRotationOffset(curve, propertyId);

    unsigned long lowest = keyTimes[0];
    unsigned long duration = keyTimes[keyCount-1] - lowest;

    float* normalizedKeyTimes = new float[keyCount];

    normalizedKeyTimes[0] = 0.0f;
    curve->setPoint(0, normalizedKeyTimes[0], keyValues, (Curve::InterpolationType) type);

    unsigned int pointOffset = propertyComponentCount;
    unsigned int i = 1;
    for (; i < keyCount - 1; i++)
    {
        normalizedKeyTimes[i] = (float) (keyTimes[i] - lowest) / (float) duration;
        curve->setPoint(i, normalizedKeyTimes[i], (keyValues + pointOffset), (Curve::InterpolationType) type);
        pointOffset += propertyComponentCount;
    }
    i = keyCount - 1;
    normalizedKeyTimes[i] = 1.0f;
    curve->setPoint(i, normalizedKeyTimes[i], keyValues + pointOffset, (Curve::InterpolationType) type);

    SAFE_DELETE(normalizedKeyTimes);

    Channel* channel = new Channel(this, target, propertyId, curve, duration);
    addChannel(channel);
    return channel;
}

Animation::Channel* Animation::createChannel(AnimationTarget* target, int propertyId, unsigned int keyCount, unsigned long* keyTimes, float* keyValues, float* keyInValue, float* keyOutValue, unsigned int type)
{
    unsigned int propertyComponentCount = target->getAnimationPropertyComponentCount(propertyId);
    assert(propertyComponentCount > 0);

    Curve* curve = new Curve(keyCount, propertyComponentCount);
    if (target->_targetType == AnimationTarget::TRANSFORM)
        setTransformRotationOffset(curve, propertyId);
    
    unsigned long lowest = keyTimes[0];
    unsigned long duration = keyTimes[keyCount-1] - lowest;

    float* normalizedKeyTimes = new float[keyCount];
    
    normalizedKeyTimes[0] = 0.0f;
    curve->setPoint(0, normalizedKeyTimes[0], keyValues, (Curve::InterpolationType) type, keyInValue, keyOutValue);

    unsigned int pointOffset = propertyComponentCount;
    unsigned int i = 1;
    for (; i < keyCount - 1; i++)
    {
        normalizedKeyTimes[i] = (float) (keyTimes[i] - lowest) / (float) duration;
        curve->setPoint(i, normalizedKeyTimes[i], (keyValues + pointOffset), (Curve::InterpolationType) type, (keyInValue + pointOffset), (keyOutValue + pointOffset));
        pointOffset += propertyComponentCount;
    }
    i = keyCount - 1;
    normalizedKeyTimes[i] = 1.0f;
    curve->setPoint(i, normalizedKeyTimes[i], keyValues + pointOffset, (Curve::InterpolationType) type, keyInValue + pointOffset, keyOutValue + pointOffset);

    SAFE_DELETE(normalizedKeyTimes);

    Channel* channel = new Channel(this, target, propertyId, curve, duration);
    addChannel(channel);
    return channel;
}

void Animation::addChannel(Channel* channel)
{
    _channels.push_back(channel);
    
    if (channel->_duration > _duration)
        _duration = channel->_duration;
}

void Animation::removeChannel(Channel* channel)
{
    std::vector<Animation::Channel*>::iterator itr = _channels.begin();
    while (itr != _channels.end())
    {
        Animation::Channel* chan = *itr;
        if (channel == chan) 
        {
            _channels.erase(itr);
            itr = _channels.end();
        }
        else
        {
            itr++;
        }
    }

    if (_channels.empty())
        _controller->destroyAnimation(this);
}

void Animation::setTransformRotationOffset(Curve* curve, unsigned int propertyId)
{
    switch (propertyId)
    {
    case Transform::ANIMATE_ROTATE:
    case Transform::ANIMATE_ROTATE_TRANSLATE:
        curve->setQuaternionOffset(ANIMATION_ROTATE_OFFSET);
        return;
    case Transform::ANIMATE_SCALE_ROTATE_TRANSLATE:
        curve->setQuaternionOffset(ANIMATION_SRT_OFFSET);
        return;
    }

    return;
}

}
