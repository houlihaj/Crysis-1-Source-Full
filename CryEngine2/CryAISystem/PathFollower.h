#ifndef PATHFOLLOWER_H
#define PATHFOLLOWER_H

/// This implements path following in a rather simple - and therefore cheap way. One of the 
/// results is an ability to generate a pretty accurate prediction of the future entity position,
/// assuming that the entity follows the output of this almost exactly.
class CPathFollower
{
public:
  struct SPathFollowerParams
  {
    SPathFollowerParams(float normalSpeed = 0.0f, float pathRadius = 0.0f, float pathLookAheadDist = 1.0f, 
      float maxAccel = 0.0f, float minSpeed = 0.0f, float maxSpeed = 10.0f, 
      float endDistance = 0.0f, bool stopAtEnd = true, bool use2D = true)
      : normalSpeed(normalSpeed), pathRadius(pathRadius), pathLookAheadDist(pathLookAheadDist), 
      maxAccel(maxAccel), minSpeed(minSpeed), maxSpeed(maxSpeed),
      endDistance(endDistance), stopAtEnd(stopAtEnd), use2D(use2D) {maxDecel = 1.f;}
    
    float normalSpeed; ///< normal entity speed
    float pathRadius; ///< max deviation allowed from the path
    float pathLookAheadDist; ///< how far we look ahead along the path - normally the same as pathRadius
    float maxAccel; ///< maximum acceleration of the entity
    float maxDecel; ///< maximum deceleration of the entity
    float minSpeed; ///< minimum output speed (unless it's zero on path end etc)
    float maxSpeed; ///< maximum output speed
    float endDistance; ///< stop this much before the end
    bool stopAtEnd; ///< aim to finish the path by reaching the end position (stationary), or simply overshoot
    bool use2D; ///< follow in 2 or 3D
    void Serialize(TSerialize ser);
  };

  CPathFollower(const SPathFollowerParams &params = SPathFollowerParams());
  ~CPathFollower();

  /// This attaches us to a particular path (pass 0 to detach)
  void AttachToPath(CNavPath *navPath);

  /// The params may be modified as the path is followed - bear in mind that doing so
  /// will invalidate any predictions
  SPathFollowerParams &GetParams() {return m_params;}
  /// Just view the params
  const SPathFollowerParams &GetParams() const {return m_params;}

  struct SPathFollowResult
  {
    struct SPredictedState {
      SPredictedState() {}
      SPredictedState(const Vec3 &p, const Vec3 &v) : pos(p), vel(v) {}
      Vec3 pos; Vec3 vel;
    };
    typedef std::vector<SPredictedState> TPredictedStates;

    /// maximum time to predict out to - actual prediction may not go this far
    float desiredPredictionTime;
    /// the first element in predictedStates will be now + predictionDeltaTime, etc
    float predictionDeltaTime;
    /// if this is non-zero then on output the prediction will be placed into it
    TPredictedStates *predictedStates;

    SPathFollowResult() 
      : predictionDeltaTime(0.1f), predictedStates(0), desiredPredictionTime(0) {}

    bool reachedEnd;
    Vec3 velocityOut;
  };

  /// Updates the path following progress, resulting in a move instruction and prediction of future
  /// states if desired.
  void Update(SPathFollowResult &result, const Vec3 &curPos, const Vec3 &curVel, float dt);

  /// Advances the current state in terms of position - effectively pretending that the follower
  /// has gone further than it has.
  void Advance(float distance);

  /// Returns the distance from the lookahead to the end, plus the distance from the position passed in
  /// to the LA if pCurPos != 0
  float GetDistToEnd(const Vec3 *pCurPos) const;

  void Serialize(TSerialize ser, class CObjectTracker& objectTracker, CNavPath *navPath);

  void Draw(IRenderer *pRenderer, const Vec3& drawOffset=ZERO) const;

  /// Returns the distance along the path from the current look-ahead position to the 
  /// first smart object path segment. If there's no path, or no smart objects on the 
  /// path, then std::numeric_limits<float>::max() will be returned
  float GetDistToSmartObject() const;

	/// Returns a point on the path some distance ahead. actualDist is set according to 
  /// how far we looked - may be less than dist if we'd reach the end
	Vec3 GetPathPointAhead(float dist, float &actualDist) const;

private:

  /// The control points are attached to the original path, but can be dynamically adjusted
  /// according to the pathRadius. Thus the offsetAmount is scaled by pathRadius when calculating 
  /// the actual position
  struct SPathControlPoint
  {
    SPathControlPoint(IAISystem::ENavigationType navType, const Vec3 &pos, const Vec3 &offsetDir, float offsetAmount)
      : navType(navType), pos(pos), offsetDir(offsetDir), offsetAmount(offsetAmount) {}

    IAISystem::ENavigationType navType;
    Vec3 pos;
    Vec3 offsetDir;
    float offsetAmount;
  };
  typedef std::vector<SPathControlPoint> TPathControlPoints;

  Vec3 GetPathControlPoint(unsigned iPt) const {
    return m_pathControlPoints[iPt].pos + m_pathControlPoints[iPt].offsetDir * (m_pathControlPoints[iPt].offsetAmount * m_params.pathRadius);}

  /// calculates distance between two points, depending on m_params.use_2D
  float DistancePointPoint(const Vec3 pt1, const Vec3 pt2) const;
  /// calculates distance squared between two points, depending on m_params.use_2D
  float DistancePointPointSq(const Vec3 pt1, const Vec3 pt2) const;

  /// Used internally to create our list of path control points
  void ProcessPath();

  /// calculates a suitable starting state of the lookahead (etc) to match curPos
  void StartFollowing(const Vec3 &curPos, const Vec3 &curVel);

  /// Used internally to calculate a new lookahead position. const so that it can be used in prediction
  /// where we integrate forwards in time.
  /// curPos and curVel are the position/velocity of the follower
  void GetNewLookAheadPos(Vec3 &newLAPos, int &newLASegIndex, const Vec3 &curLAPos, int curLASegIndex, 
    const Vec3 &curPos, const Vec3 &curVel, float dt) const;

  /// uses the lookahead and current position to derive a velocity (ultimately using speed control etc) that 
  /// should be applied (i.e. passed to physics) to curPos.
  void UseLookAheadPos(Vec3 &velocity, bool &reachedEnd, const Vec3 LAPos, const Vec3 curPos, const Vec3 curVel, 
    float dt, int curLASegmentIndex, float &lastOutputSpeed) const;

  /// As public version but you pass the current state in
  Vec3 GetPathPointAhead(float dist, float &actualDist, int curLASegmentIndex, Vec3 curLAPos) const;

  /// As public version but you pass the current state in
  float GetDistToEnd(const Vec3 *pCurPos, int curLASegmentIndex, Vec3 curLAPos) const;

  TPathControlPoints m_pathControlPoints;

  /// parameters describing how we follow the path - can be modified as we follow
  SPathFollowerParams m_params;

  /// segment that we're on - goes from this point index to the next
  int m_curLASegmentIndex;

  /// position of our lookahead point - the thing we aim towards
  Vec3 m_curLAPos;

  /// used for controlling the acceleration
  float m_lastOutputSpeed;

  CNavPath *m_navPath;

  int m_pathVersion;
};

#endif