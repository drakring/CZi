/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Center of Advanced European Studies and Research (caesar)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

// cpp wrapper for libCZI for reading Zeiss czi file scene data.

#include "Python.h"
#define NPY_NO_DEPRECATED_API NPY_1_14_API_VERSION
#include "numpy/arrayobject.h"

#include <iostream>
//#include <assert.h>
#include <vector>

using namespace std;

#include "inc_libCZI.h"

using namespace libCZI;

// https://stackoverflow.com/questions/3342726/c-print-out-enum-value-as-text
std::ostream& operator<<(std::ostream& out, const libCZI::PixelType value){
    static std::map<libCZI::PixelType, std::string> strings;
    if (strings.size() == 0){
#define INSERT_ELEMENT(p) strings[p] = #p
        INSERT_ELEMENT(libCZI::PixelType::Invalid);     
        INSERT_ELEMENT(libCZI::PixelType::Gray8);
        INSERT_ELEMENT(libCZI::PixelType::Gray16);
        INSERT_ELEMENT(libCZI::PixelType::Gray32Float);
        INSERT_ELEMENT(libCZI::PixelType::Bgr24);
        INSERT_ELEMENT(libCZI::PixelType::Bgr48);
        INSERT_ELEMENT(libCZI::PixelType::Bgr96Float);
        INSERT_ELEMENT(libCZI::PixelType::Bgra32);
        INSERT_ELEMENT(libCZI::PixelType::Gray64ComplexFloat);
        INSERT_ELEMENT(libCZI::PixelType::Bgr192ComplexFloat);
        INSERT_ELEMENT(libCZI::PixelType::Gray32);
        INSERT_ELEMENT(libCZI::PixelType::Gray64Float);
#undef INSERT_ELEMENT
    }   

    return out << strings[value];
}

std::ostream& operator<<(std::ostream& out, const libCZI::DimensionIndex value){
    static std::map<libCZI::DimensionIndex, std::string> strings;
    if (strings.size() == 0){
#define INSERT_ELEMENT(p) strings[p] = #p
        INSERT_ELEMENT(libCZI::DimensionIndex::invalid);
        INSERT_ELEMENT(libCZI::DimensionIndex::MinDim);
        INSERT_ELEMENT(libCZI::DimensionIndex::Z);
        INSERT_ELEMENT(libCZI::DimensionIndex::C);
        INSERT_ELEMENT(libCZI::DimensionIndex::T);
        INSERT_ELEMENT(libCZI::DimensionIndex::R);
        INSERT_ELEMENT(libCZI::DimensionIndex::S);
        INSERT_ELEMENT(libCZI::DimensionIndex::I);
        INSERT_ELEMENT(libCZI::DimensionIndex::H);
        INSERT_ELEMENT(libCZI::DimensionIndex::V);
        INSERT_ELEMENT(libCZI::DimensionIndex::B);
        INSERT_ELEMENT(libCZI::DimensionIndex::MaxDim);
#undef INSERT_ELEMENT
    }

    return out << strings[value];
}

/* #### Globals #################################### */

// .... Python callable extensions ..................

static PyObject *cziread_meta(PyObject *self, PyObject *args);
static PyObject *cziread_scene(PyObject *self, PyObject *args);
static PyObject *cziread_allsubblocks(PyObject *self, PyObject *args);

/* ==== Set up the methods table ====================== */
static PyMethodDef _pylibcziMethods[] = {
    {"cziread_meta", cziread_meta, METH_VARARGS, "Read czi meta data"},
    {"cziread_scene", cziread_scene, METH_VARARGS, "Read czi scene image"},
    {"cziread_allsubblocks", cziread_allsubblocks, METH_VARARGS, "Read czi image containing all scenes"},

    {NULL, NULL, 0, NULL}        /* Sentinel */
};

// https://docs.python.org/3.6/extending/extending.html
// http://python3porting.com/cextensions.html

static struct PyModuleDef moduledef = {
        PyModuleDef_HEAD_INIT,
        "_pylibczi",           /* m_name */
        NULL,                /* m_doc */
        -1,                  /* m_size */
        _pylibcziMethods,      /* m_methods */
        NULL,                /* m_reload */
        NULL,                /* m_traverse */
        NULL,                /* m_clear */
        NULL                 /* m_free */
};

// generic exception for any errors encountered here
static PyObject *PylibcziError;

extern "C" {
PyMODINIT_FUNC PyInit__pylibczi(void)
{
    PyObject *module = PyModule_Create(&moduledef);

    if (module == NULL)
        return NULL;

    PylibcziError = PyErr_NewException("pylibczi.error", NULL, NULL);
    Py_INCREF(PylibcziError);
    PyModule_AddObject(module, "_pylibczi_exception", PylibcziError);
    
    import_array();  // Must be present for NumPy.  Called first after above line.

    return module;
}
}

/* #### Helper prototypes ################################### */

std::shared_ptr<ICZIReader> open_czireader_from_cfilename(char const *fn);
PyArrayObject* copy_bitmap_to_numpy_array(std::shared_ptr<libCZI::IBitmapData> pBitmap);

/* #### Extended modules #################################### */

static PyObject *cziread_meta(PyObject *self, PyObject *args) {
    char *filename_buf;
    // parse arguments
    if (!PyArg_ParseTuple(args, "s", &filename_buf)) 
        return NULL;

    auto cziReader = open_czireader_from_cfilename(filename_buf);
        
    // get the the document's metadata
    auto mds = cziReader->ReadMetadataSegment();
    auto md = mds->CreateMetaFromMetadataSegment();
    //auto docInfo = md->GetDocumentInfo();
    //auto dsplSettings = docInfo->GetDisplaySettings();
    std::string xml = md->GetXml();
    // copy the metadata into python string
    PyObject* pystring = Py_BuildValue("s", xml.c_str());
    
    cziReader->Close();
    return pystring;
}

static PyObject *cziread_allsubblocks(PyObject *self, PyObject *args) {
    char *filename_buf;
    // parse arguments
    if (!PyArg_ParseTuple(args, "s", &filename_buf)) 
        return NULL;

    auto cziReader = open_czireader_from_cfilename(filename_buf);

    // count all the subblocks
    npy_intp subblock_count = 0;
    cziReader->EnumerateSubBlocks(
        [&subblock_count](int idx, const libCZI::SubBlockInfo& info)
    {
        subblock_count++;
        return true;
    });
    //cout << "Enumerated " << subblock_count << endl;
    
    // meh - this seems to be not useful, what is an M-index? someone read the spec...
    //auto stats = cziReader->GetStatistics();
    //cout << stats.subBlockCount << " " << stats.maxMindex << endl;
    //int subblock_count = stats.subBlockCount;

    // copy the image data and coordinates into numpy arrays, return images as python list of numpy arrays
    PyObject* images = PyList_New(subblock_count);
    npy_intp eshp[2]; eshp[0] = subblock_count; eshp[1] = 2;
    PyArrayObject *coordinates = (PyArrayObject *) PyArray_Empty(2, eshp, PyArray_DescrFromType(NPY_INT32), 0);
    npy_int32 *coords = (npy_int32 *) PyArray_DATA(coordinates);

    npy_intp cnt = 0;
    cziReader->EnumerateSubBlocks(
        [&cziReader, &subblock_count, &cnt, images, coords](int idx, const libCZI::SubBlockInfo& info)
    {
        //cout << "Index " << idx << ": " << libCZI::Utils::DimCoordinateToString(&info.coordinate) 
        //  << " Rect=" << info.logicalRect << endl;
                
        // add the sub-block image
        PyList_SetItem(images, cnt, 
            (PyObject*) copy_bitmap_to_numpy_array(cziReader->ReadSubBlock(idx)->CreateBitmap()));
        // add the coordinates
        coords[2*cnt] = info.logicalRect.x; coords[2*cnt+1] = info.logicalRect.y;
        
        cnt++;
        return true;
    });

    return Py_BuildValue("OO", images, (PyObject *) coordinates);
}

static PyObject *cziread_scene(PyObject *self, PyObject *args) {
    char *filename_buf;
    PyArrayObject *scene_or_box;

    // parse arguments
    if (!PyArg_ParseTuple(args, "sO!", &filename_buf, &PyArray_Type, &scene_or_box)) 
        return NULL;    

    // get either the scene or a bounding box on the scene to load
    npy_intp size_scene_or_box = PyArray_SIZE(scene_or_box);
    if( PyArray_TYPE(scene_or_box) != NPY_INT64 ) {
        PyErr_SetString(PylibcziError, "Scene or box argument must be int64");
        return NULL;
    }
    npy_int64 *ptr_scene_or_box = (npy_int64*) PyArray_DATA(scene_or_box);
    bool use_scene; npy_int32 scene; npy_int32 rect[4];
    if( size_scene_or_box == 1 ) {
        use_scene = true;
        scene = ptr_scene_or_box[0];
    } else if( size_scene_or_box == 4 ) {
        use_scene = false;
        for( int i=0; i < 4; i++ ) rect[i] = ptr_scene_or_box[i];
    } else {
        PyErr_SetString(PylibcziError, "Second input must be size 1 (scene) or 4 (box)");
        return NULL;
    }

    auto cziReader = open_czireader_from_cfilename(filename_buf);

    // if only the scene was given the enumerate subblocks to get limits, otherwise use the provided bounding box.
    int min_x, min_y, max_x, max_y, size_x, size_y;
    //std::vector<bool> valid_dims ((int) libCZI::DimensionIndex::MaxDim, false);
    if( use_scene ) {
        // enumerate subblocks, get the min and max coordinates of the specified scene
        min_x = std::numeric_limits<int>::max(); min_y = std::numeric_limits<int>::max(); max_x = -1; max_y = -1;
        cziReader->EnumerateSubBlocks(
            //[scene, &min_x, &min_y, &max_x, &max_y, &valid_dims](int idx, const libCZI::SubBlockInfo& info)
            [scene, &min_x, &min_y, &max_x, &max_y](int idx, const libCZI::SubBlockInfo& info)
        {
            int cscene = 0;
            info.coordinate.TryGetPosition(libCZI::DimensionIndex::S, &cscene);
            // negative value for scene indicates to load all scenes
            if( cscene == scene || scene < 0 ) {
                //cout << "Index " << idx << ": " << libCZI::Utils::DimCoordinateToString(&info.coordinate) 
                //  << " Rect=" << info.logicalRect << " scene " << scene << endl;
                auto rect = info.logicalRect;
                if( rect.x < min_x ) min_x = rect.x;
                if( rect.y < min_y ) min_y = rect.y;
                if( rect.x + rect.w > max_x ) max_x = rect.x + rect.w;
                if( rect.y + rect.h > max_y ) max_y = rect.y + rect.h;
            }
            
            //info.coordinate.EnumValidDimensions(
            //    [&valid_dims](libCZI::DimensionIndex dim, int value)
            //{
            //    valid_dims[(int) dim] = true;
            //    //cout << "Dimension  " << dim << " value " << value << endl;
            //    return true;
            //});
            
            return true;
        });
        size_x = max_x-min_x; size_y = max_y-min_y;
    } else {
        min_x = rect[0]; size_x = rect[2]; min_y = rect[1]; size_y = rect[3];
        max_x = min_x + size_x; max_y = min_y + size_y;
    }
    //cout << "min x y " << min_x << " " << min_y << " max x y " << max_x << " " << max_y << endl;
    //for (auto it = valid_dims.begin(); it != valid_dims.end(); ++it) {
    //    if( *it ) {
    //        int index = std::distance(valid_dims.begin(), it);
    //        std::cout << static_cast<libCZI::DimensionIndex>(index) << ' ';
    //    }
    //}
    //cout << endl;
    
    // get the accessor to the image data
    auto accessor = cziReader->CreateSingleChannelTileAccessor();
    // xxx - how to generalize correct image dimension here?
    //   commented code above creates bool vector saying which dims are valid (in any subblock).
    //   it is possible for a czi file to not have any valid dims, not sure what this means exactly.
    //libCZI::CDimCoordinate planeCoord{ { libCZI::DimensionIndex::Z,0 } };
    libCZI::CDimCoordinate planeCoord{ { libCZI::DimensionIndex::C,0 } };
    auto multiTileComposit = accessor->Get(
        libCZI::IntRect{ min_x, min_y, size_x, size_y },
        &planeCoord,
        nullptr);   // use default options

    PyArrayObject* img = copy_bitmap_to_numpy_array(multiTileComposit);

    cziReader->Close();
    return (PyObject*) img;
}

PyArrayObject* copy_bitmap_to_numpy_array(std::shared_ptr<libCZI::IBitmapData> pBitmap) {
    // allocate the matlab matrix to copy image into
    int numpy_type = NPY_UINT16; int pixel_size_bytes = 0;
    //cout << pBitmap->GetPixelType() << endl;
    switch( pBitmap->GetPixelType() ) {
        case libCZI::PixelType::Gray8:
            numpy_type = NPY_UINT8; pixel_size_bytes = 1;
            break;
        case libCZI::PixelType::Gray16:
            numpy_type = NPY_UINT16; pixel_size_bytes = 2;
            break;
        default:
            PyErr_SetString(PylibcziError, "Unknown image type in czi file, ask to add more types.");
            return NULL;
    }
    
    /* Create an m-by-n numpy ndarray. */
    //cout << size_x << " " << size_y << endl;
    auto size = pBitmap->GetSize();
    int size_x = size.w, size_y = size.h;    
    npy_intp shp[2]; shp[0] = size_x; shp[1] = size_y;
    // images in czi file are in F-order, set F-order flag
    PyArrayObject *img = (PyArrayObject *) PyArray_Empty(2, shp, PyArray_DescrFromType(numpy_type), 1);
    void *pointer = PyArray_DATA(img);
        
    // copy from the czi lib image pointer to the numpy array pointer
    auto bitmap = pBitmap->Lock(); 
    //cout << "sixe_x " << size_x << " size y " << size_y << endl;
    //cout << "stride " << bitmap.stride << " size " << bitmap.size << endl;
    // can not do a single memcpy call because the stride does not necessarily match the row size.
    npy_byte *cptr = (npy_byte*)pointer, *cimgptr = (npy_byte*)bitmap.ptrDataRoi;
    // stride units is not documented but emperically means the row (x) stride in bytes, not in pixels.
    int rowsize = pixel_size_bytes * size_x; //, imgrowsize = pixel_size_bytes * bitmap.stride;
    for( int y=0; y < size_y; y++ ) {
        std::memcpy(cptr, cimgptr, rowsize);
        cptr += rowsize; cimgptr += bitmap.stride;
    }
    pBitmap->Unlock();
    
    // to be compatible with other Zeiss software, tranpose the axes for image space
    return (PyArrayObject*) PyArray_SwapAxes(img,0,1);
}

std::shared_ptr<ICZIReader> open_czireader_from_cfilename(char const *fn) {
    // open the czi file
    // https://msdn.microsoft.com/en-us/library/ms235631.aspx
    size_t newsize = strlen(fn) + 1;
    // The following creates a buffer large enough to contain   
    // the exact number of characters in the original string  
    // in the new format. If you want to add more characters  
    // to the end of the string, increase the value of newsize  
    // to increase the size of the buffer.  
    wchar_t * wcstring = new wchar_t[newsize];
    // Convert char* string to a wchar_t* string.  
    //size_t convertedChars = mbstowcs(wcstring, fn, newsize);
    mbstowcs(wcstring, fn, newsize);
    auto cziReader = libCZI::CreateCZIReader();
    auto stream = libCZI::CreateStreamFromFile(wcstring);
    delete[] wcstring;
    cziReader->Open(stream);

    return cziReader;
}