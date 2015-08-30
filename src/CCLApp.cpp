#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/CameraUi.h"
#include "cinder/Json.h"

#include "CCL_MocapData.h"
#include "Skeleton.h"

using namespace ci;
using namespace ci::app;
using namespace std;

//GLOBAL VARIABLES
//const float DRAW_SCALE = 200;       //SCALE FOR DRAWING THE POINTS
int TOTAL_FRAMES;

class CCLApp : public App {
  public:
   // static void prepareSettings( Settings *settings );

	void setup() override;
	void mouseDown( MouseEvent event ) override;
    void mouseDrag (MouseEvent event) override;
	void update() override;
	void draw() override;

    void setupEnviron( int xSize, int zSize, int spacing );     //METHOD FOR SETTING UP THE 3D ENVIRONMENT
    void renderScene();                                         //METHOD FOR RENDERING THE 3D ENVIRONMENT
    void initData();                                           //METHOD FOR IMPORTING JSON DATA
    void setupShader();

    //CREATE A BATCH VERTEX FOR THE FLOOR MESH
    gl::VertBatchRef	mGridMesh;

    CameraPersp			mCamera;
    CameraUi			mCamUi;

    std::vector<CCL_MocapJoint> jointList;  //GLOBAL CONTAINER TO HOLD THE JOINT POSITIONS

    //TEST SPHERES
    gl::BatchRef        mSphereBatch;
    gl::VboRef			mInstanceDataVbo;

    gl::GlslProgRef		mGlsl;
    gl::GlslProgRef     solidShader;

    int numJoints;

    // create an array of initial per-instance positions laid out in a 2D grid
    //std::vector<glm::vec3> jointPositions;

    //CREATE A CONTAINER TO STORE THE POSITION OF EACH JOINT FOR A SINGLE FRAME
    std::vector<glm::vec3> framePositions;


    typedef vector<glm::vec3>::size_type bodySize;
 //   bodySize sizeOfBody = jointPositions.size();

    Skeleton skeleton;
    int      mCurrentFrame = 0;
    size_t   frameIndex(int frame);
    vector<size_t> orderedIndices;

    vector<size_t> generateOrderedIndices(const std::vector<CCL_MocapJoint> &joints);
};


//--------------------- SETUP ----------------------------------

size_t CCLApp::frameIndex(int frame)
{
  if (orderedIndices.empty())
  {
    return frame;
  }
  else
  {
    return orderedIndices.at(frame);
  }
}

void CCLApp::setup()
{

    setFrameRate(12);

    //SETUP THE 3D ENVIRONMENT
    setupEnviron( 2000, 2000, 50 );

    //SETUP THE CAMERA
    mCamera.lookAt( vec3( 0, 500, 0 ), vec3( -1888.450,  142.485, -891.197 ) );

    mCamera.setFarClip(10000);

    //mCamera.setEyePoint(vec3(0,200,650));
    mCamUi = CameraUi( &mCamera, getWindow() );

    setupShader();

    initData(); //IMPORT THE JSON DATA AND SORT IT INTO A LIST


    /* THIS JUST PRINTS OUT THE POSITIONS OF THE FIRST JOINT TO CHECK THAT IT'S LOADED CORRECTLY
    for (int i = 0; i < 1; i++){
        for (int j = 0; j < jointList[i].jointPositions.size(); j++){
            std::cout << j << ": " << jointList[i].jointPositions[j] << std::endl;
        }
    }
    */


    mCurrentFrame = 0;
    TOTAL_FRAMES = jointList[0].jointPositions.size(); //SHOULD PROBABLY PUT A TRY/CATCH HERE

    std::cout << "total frames: " << TOTAL_FRAMES << std::endl;

    gl::VboMeshRef body = gl::VboMesh::create( geom::Sphere().subdivisions( 16 ).radius(2) );


    // CREATE THE SPHERES AT THE INITIAL JOINT LOCATIONS
    for ( int i = 0; i < jointList.size(); ++i ) {
        float instanceX = jointList[i].jointPositions[0].x;
        float instanceY = jointList[i].jointPositions[0].y;
        float instanceZ = jointList[i].jointPositions[0].z;
       // float instanceZ = 0;

        framePositions.push_back( vec3( instanceX, instanceY, instanceZ));
    }

    skeleton = Skeleton(framePositions);

    //std::cout << "positions: " << positions[0] << std::endl;



    // create the VBO which will contain per-instance (rather than per-vertex) data
    mInstanceDataVbo = gl::Vbo::create( GL_ARRAY_BUFFER, framePositions.size() * sizeof(vec3), framePositions.data(), GL_DYNAMIC_DRAW );

    // we need a geom::BufferLayout to describe this data as mapping to the CUSTOM_0 semantic, and the 1 (rather than 0) as the last param indicates per-instance (rather than per-vertex)
    geom::BufferLayout instanceDataLayout;

    instanceDataLayout.append( geom::Attrib::CUSTOM_0, 3, 0, 0, 1 /* per instance */ );

    //NOW ADD IT TO THE VBO MESH THAT WE INITIAL CREATED FOR THE BODY / SKELETON
    body->appendVbo( instanceDataLayout, mInstanceDataVbo );

    //FINALLY, BUILD THE BATCH, AND MAP THE CUSTOM_0 ATTRIBUTE TO THE "vInstancePosition" GLSL VERTEX ATTRIBUTE
    mSphereBatch = gl::Batch::create( body, mGlsl, { { geom::Attrib::CUSTOM_0, "vInstancePosition" } } );

    gl::enableDepthWrite();
    gl::enableDepthRead();

    //PRINT OUT JOINT INDEX AND NAME OF JOINT

    for (int i = 0; i < jointList.size(); i++)
    {
        std::cout << "index: " << i << ", Joint name: " << jointList[i].jointName << std::endl;
    }


  orderedIndices = generateOrderedIndices(jointList);
}

vector<size_t> CCLApp::generateOrderedIndices(const std::vector<CCL_MocapJoint> &joints)
{
  auto count = joints.at(0).jointPositions.size();
  vector<size_t> indices;
  indices.reserve(count);
  for (auto i = 0; i < count; i += 1) {
    indices.push_back(i);
  }

  auto sort_joint = 27;
  std::stable_sort(indices.begin(), indices.end(), [&] (size_t lhs, size_t rhs) {
    //
    auto lhs_joint = joints.at(sort_joint).jointPositions.at(lhs);
    auto rhs_joint = joints.at(sort_joint).jointPositions.at(rhs);

    return lhs_joint.x < rhs_joint.x;
  });

  return indices;
}

//--------------------- MOUSEDOWN ------------------------------

void CCLApp::mouseDown( MouseEvent event )
{
}


//----------------------- UPDATE -------------------------------

void CCLApp::update()
{
  //  std::cout << mCamera.getEyePoint() << std::endl;

    //UPDATE POSITIONS
    //MAP INSTANCE DATA TO VBO
    //WRITE NEW POSITIONS
    //UNMAP

    glm::vec3 *newPositions = (glm::vec3*)mInstanceDataVbo->mapReplace();

    for( int i = 0; i < jointList.size(); ++i ) {

        float instanceX = jointList[i].jointPositions[frameIndex(mCurrentFrame)].x;
        float instanceY = jointList[i].jointPositions[frameIndex(mCurrentFrame)].y;
        float instanceZ = jointList[i].jointPositions[frameIndex(mCurrentFrame)].z;
        //float instanceZ = 0;
            // just some nonsense math to move the teapots in a wave
            vec3 newPos(vec3(instanceX,instanceY, instanceZ));


        if (i == 27){
            std::cout << newPos << endl;
        }
        framePositions[i] = newPos;
        *newPositions++ = newPos;

    }

    skeleton.update(framePositions);


    mInstanceDataVbo->unmap();
   // std::cout << "position: " << positions[0] << std::endl;



    //MANUALLY INCREMENT THE FRAME, IF THE mCurrentFrame EXCEEDS TOTAL FRAMES, RESET THE COUNTER
    if (mCurrentFrame < TOTAL_FRAMES)
    {
      mCurrentFrame += 1;
    } else {
      mCurrentFrame = 0;
    }

   // std::cout << "frame rate: " << getAverageFps() << ", frame count: " << mCurrentFrame << std::endl;
}

//--------------------------- DRAW -----------------------------

void CCLApp::draw()
{

    gl::clear(Color(0.1f,0.1f,0.1f) );

    gl::setMatrices( mCamera );



    renderScene();

    gl::color( 1, 0, 0 );
    //gl::ScopedModelMatrix modelScope;
    //mSphereBatch->drawInstanced( sizeOfBody );

    mSphereBatch->drawInstanced( jointList.size() );
     skeleton.render();


}

//------------------- MOUSE DRAGGED ------------------------------

void CCLApp::mouseDrag( MouseEvent event )
{
    mCamUi.mouseDrag( event );
}

//------------------- SETUP THE ENVIRONMENT / GRID -----------------------

void CCLApp::setupEnviron( int xSize, int zSize, int spacing )
{
    CI_ASSERT( ( spacing <= xSize ) && ( spacing <= zSize ) );

    // Cut in half and adjust for spacing.
    xSize = ( ( xSize / 2 ) / spacing ) * spacing;
    zSize = ( ( zSize / 2 ) / spacing ) * spacing;

    const int xMax = xSize + spacing;
    const int zMax = zSize + spacing;
    const ColorA defaultColor( 0.9f, 0.9f, 0.9f,0.1f);
    const ColorA black( 0, 0, 0, 1 );

    mGridMesh = gl::VertBatch::create( GL_LINES );

    // Add x lines.
    for( int xVal = -xSize; xVal < xMax; xVal += spacing ) {
        mGridMesh->color( defaultColor );
        mGridMesh->vertex( (float)xVal, 0, (float)-zSize );
        mGridMesh->vertex( (float)xVal, 0, (float)zSize );
    }// end for each x dir line

    // Add z lines.
    for( int zVal = -zSize; zVal < zMax; zVal += spacing ) {
        mGridMesh->color( defaultColor );
        mGridMesh->vertex( (float)xSize, 0, (float)zVal );
        mGridMesh->vertex( (float)-xSize, 0, (float)zVal );
    }// end for each z dir line
}


//------------------ RENDER THE SCENE ------------------------

void CCLApp::renderScene()
{

   // gl::pushMatrices();
    mGridMesh->draw();

  //  gl::popMatrices();
}

//-------------------- IMPORT DATA -------------------------

void CCLApp::initData()
{
    //CREATE AND INITIALISE A CCL_MOCAPDATA OBJECT, PASSING IN THE GLOBAL "jointList" AS A REFERENCE
  jointList = ccl::loadMotionCaptureFromJson(getAssetPath("CCL_JOINT.json"));
//  jointList = ccl::loadMotionCaptureFromSite(Url(ccl::URL_STREAM_JSON), 1);
 //    CCL_MocapData(1, jointList); //UNCOMMENT THIS LINE TO CAPTURE NEW JSON DATA
     std::cout << jointList.size()<< endl;
     std::cout << endl;
     std::cout << endl;
     std::cout << endl;


}

//--------------------- SETUP SHADERS -----------------------

void CCLApp::setupShader(){

    //CHOOSE BETWEEN solidShader AND mGlsl AS SHADERS FOR THE SPHERES
    solidShader = gl::getStockShader( gl::ShaderDef().color() );
    mGlsl = gl::GlslProg::create( loadAsset( "shader.vert" ), loadAsset( "shader.frag" ) );
    mSphereBatch = gl::Batch::create( geom::Sphere(), solidShader );
}

//-------------------------------------------------------------

CINDER_APP( CCLApp, RendererGl, [&]( App::Settings *settings ) {
    settings->setWindowSize( 1280, 720 );
} )
