// Copyright (C) 2018 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

/**
 * @brief The entry point for inference engine deconvolution sample application
 * @file style_transfer_sample/main.cpp
 * @example style_transfer_sample/main.cpp
 */
#include <fstream>
#include <iomanip>
#include <vector>
#include <string>
#include <chrono>
#include <memory>

#include <format_reader_ptr.h>
#include <inference_engine.hpp>
#include <ext_list.hpp>

#include <samples/common.hpp>
#include <samples/slog.hpp>
#include <samples/args_helper.hpp>
#include <thread>

//AI_Toolkit.FST.Openvino.Samual add for opencv real time style transfer 20190305 start 
#include <samples/ocv_common.hpp>
#include <opencv2/opencv.hpp>
//AI_Toolkit.FST.Openvino.Samual add for opencv real time style transfer 20190305 end 

#include "style_transfer_sample.h"

using namespace InferenceEngine;


bool ParseAndCheckCommandLine(int argc, char *argv[]) {
    // ---------------------------Parsing and validation of input args--------------------------------------
    slog::info << "Parsing input parameters" << slog::endl;

    gflags::ParseCommandLineNonHelpFlags(&argc, &argv, true);
    if (FLAGS_h) {
        showUsage();
        return false;
    }

    if (FLAGS_ni < 1) {
        throw std::logic_error("Parameter -ni should be more than 0 !!! (default 1)");
    }

    //AI_Toolkit.FST.Openvino.Samual removed for opencv real time style transfer 20190305 start 
	if (FLAGS_i.empty()) {
    //    throw std::logic_error("Parameter -i is not set");
    	slog::info << "for live camera style transfer" << slog::endl;
    }
	//AI_Toolkit.FST.Openvino.Samual removed for opencv real time style transfer 20190305 End 


    if (FLAGS_m.empty()) {
        throw std::logic_error("Parameter -m is not set");
    }

    return true;
}

int main(int argc, char *argv[]) {
    try {
        slog::info << "InferenceEngine: " << GetInferenceEngineVersion() << slog::endl;
        // ------------------------------ Parsing and validation of input args ---------------------------------
        if (!ParseAndCheckCommandLine(argc, argv)) {
            return 0;
        }

        //AI_Toolkit.FST.Openvino.Samual removed for opencv real time style transfer 20190305 start
        //Due to loop scope, move some variable more global
        std::vector<std::string> imageNames;
        std::vector<std::shared_ptr<uint8_t>> imagesData;
        cv::VideoCapture cap;
		cv::Mat frame;
        
        /** This vector stores paths to the processed images **/
        if (!FLAGS_i.empty())
        {
        	//std::vector<std::string> imageNames;
        	parseInputFilesArguments(imageNames);
        	if (imageNames.empty()) throw std::logic_error("No suitable images were found");
        }
        //AI_Toolkit.FST.Openvino.Samual removed for opencv real time style transfer 20190305 end
        
        // -----------------------------------------------------------------------------------------------------

        // --------------------------- 1. Load Plugin for inference engine -------------------------------------
        slog::info << "Loading plugin" << slog::endl;
        InferencePlugin plugin = PluginDispatcher({FLAGS_pp, "../../../lib/intel64", ""}).getPluginByDevice(FLAGS_d);

        /** Printing plugin version **/
        printPluginVersion(plugin, std::cout);

        /** Loading default extensions **/
        if (FLAGS_d.find("CPU") != std::string::npos) {
            /**
             * cpu_extensions library is compiled from "extension" folder containing
             * custom MKLDNNPlugin layer implementations. These layers are not supported
             * by mkldnn, but they can be useful for inferring custom topologies.
            **/
            plugin.AddExtension(std::make_shared<Extensions::Cpu::CpuExtensions>());
        }

        if (!FLAGS_l.empty()) {
            // CPU(MKLDNN) extensions are loaded as a shared library and passed as a pointer to base extension
            IExtensionPtr extension_ptr = make_so_pointer<IExtension>(FLAGS_l);
            plugin.AddExtension(extension_ptr);
            slog::info << "CPU Extension loaded: " << FLAGS_l << slog::endl;
        }
        if (!FLAGS_c.empty()) {
            // clDNN Extensions are loaded from an .xml description and OpenCL kernel files
            plugin.SetConfig({{PluginConfigParams::KEY_CONFIG_FILE, FLAGS_c}});
            slog::info << "GPU Extension loaded: " << FLAGS_c << slog::endl;
        }
        // -----------------------------------------------------------------------------------------------------

        // --------------------------- 2. Read IR Generated by ModelOptimizer (.xml and .bin files) ------------
        slog::info << "Loading network files" << slog::endl;

        CNNNetReader networkReader;
        /** Read network model **/
        networkReader.ReadNetwork(FLAGS_m);

        /** Extract model name and load weights **/
        std::string binFileName = fileNameNoExt(FLAGS_m) + ".bin";
        networkReader.ReadWeights(binFileName);
        CNNNetwork network = networkReader.getNetwork();
        // -----------------------------------------------------------------------------------------------------

        // --------------------------- 3. Configure input & output ---------------------------------------------

        // --------------------------- Prepare input blobs -----------------------------------------------------
        slog::info << "Preparing input blobs" << slog::endl;

        /** Taking information about all topology inputs **/
        InputsDataMap inputInfo(network.getInputsInfo());

        if (inputInfo.size() != 1) throw std::logic_error("Sample supports topologies only with 1 input");
        auto inputInfoItem = *inputInfo.begin();

        //AI_Toolkit.FST.Openvino.Samual removed for opencv real time style transfer 20190305 start
        if (!FLAGS_i.empty())
        {
        	/** Iterate over all the input blobs **/
        	//std::vector<std::shared_ptr<uint8_t>> imagesData;

        	/** Specifying the precision of input data.
         	* This should be called before load of the network to the plugin **/
        	inputInfoItem.second->setPrecision(Precision::FP32);
        	//inputInfoItem.second->setPrecision(Precision::FP16);

        	/** Collect images data ptrs **/
		    for (auto & i : imageNames) {
		        FormatReader::ReaderPtr reader(i.c_str());
		        if (reader.get() == nullptr) {
		            slog::warn << "Image " + i + " cannot be read!" << slog::endl;
		            continue;
		        }
		        /** Store image data **/
		        std::shared_ptr<unsigned char> data(reader->getData(inputInfoItem.second->getTensorDesc().getDims()[3],
		                                                            inputInfoItem.second->getTensorDesc().getDims()[2]));
		        if (data.get() != nullptr) {
		            imagesData.push_back(data);
		        }
		    }
		    if (imagesData.empty()) throw std::logic_error("Valid input images were not found!");

        	/** Setting batch size using image count **/
        	network.setBatchSize(imagesData.size());
        	slog::info << "Batch size is " << std::to_string(network.getBatchSize()) << slog::endl;
        }
        //AI_Toolkit.FST.Openvino.Samual removed for opencv real time style transfer 20190305 End

        // ------------------------------ Prepare output blobs -------------------------------------------------
        slog::info << "Preparing output blobs" << slog::endl;

        OutputsDataMap outputInfo(network.getOutputsInfo());
        // BlobMap outputBlobs;
        std::string firstOutputName;

        const float meanValues[] = {static_cast<const float>(FLAGS_mean_val_r),
                                    static_cast<const float>(FLAGS_mean_val_g),
                                    static_cast<const float>(FLAGS_mean_val_b)};

        for (auto & item : outputInfo) {
            if (firstOutputName.empty()) {
                firstOutputName = item.first;
            }
            DataPtr outputData = item.second;
            if (!outputData) {
                throw std::logic_error("output data pointer is not valid");
            }

            item.second->setPrecision(Precision::FP32);
            //item.second->setPrecision(Precision::FP16);
        }
        // -----------------------------------------------------------------------------------------------------

        // --------------------------- 4. Loading model to the plugin ------------------------------------------
        slog::info << "Loading model to the plugin" << slog::endl;
        ExecutableNetwork executable_network = plugin.LoadNetwork(network, {});
        // -----------------------------------------------------------------------------------------------------

        // --------------------------- 5. Create infer request -------------------------------------------------
        InferRequest infer_request = executable_network.CreateInferRequest();
        // -----------------------------------------------------------------------------------------------------

        // --------------------------- 6. Prepare input --------------------------------------------------------
        /** Iterate over all the input blobs **/
        
        //AI_Toolkit.FST.Openvino.Samual remove for opencv real time style transfer 20190305 start 
		if (!FLAGS_i.empty())
		{
			for (const auto & item : inputInfo) {
		        Blob::Ptr input = infer_request.GetBlob(item.first);
		       /** Filling input tensor with images. First b channel, then g and r channels **/
		        size_t num_channels = input->getTensorDesc().getDims()[1];
		        size_t image_size = input->getTensorDesc().getDims()[3] * input->getTensorDesc().getDims()[2];

		        auto data = input->buffer().as<PrecisionTrait<Precision::FP32>::value_type*>();
		    //    //auto data = input->buffer().as<PrecisionTrait<Precision::FP16>::value_type*>();

		        /** Iterate over all input images **/
		        for (size_t image_id = 0; image_id < imagesData.size(); ++image_id) {
		            /** Iterate over all pixel in image (b,g,r) **/
		            for (size_t pid = 0; pid < image_size; pid++) {
		                /** Iterate over all channels **/
		                for (size_t ch = 0; ch < num_channels; ++ch) {
		                    /**          [images stride + channels stride + pixel id ] all in bytes            **/
		                    data[image_id * image_size * num_channels + ch * image_size + pid ] =
		                        imagesData.at(image_id).get()[pid*num_channels + ch] - meanValues[ch];
		                }
		            }
		        }
		    }
        }
        else
        {
        	cap.open(0);
			// check if we succeeded
			if (!cap.isOpened()) {
				throw std::logic_error("ERROR! Unable to open camera \n");
				return -1;
			}
        }
        //AI_Toolkit.FST.Openvino.Samual remove for opencv real time style transfer 20190305 End 
        
		//AI_Toolkit.FST.Openvino.Samual remove for opencv real time style transfer 20190305 start	
		//add forever loop for live frame get into input blob
		for (;;)
		{
			if (FLAGS_i.empty())
			{				
				// wait for a new frame from camera and store it into 'frame'
				cap.read(frame);
				// check if we succeeded
				if (frame.empty()) 
				{
					throw std::logic_error("ERROR! blank frame grabbed \n");
					break; 
				}
			
				const size_t width = frame.cols;
				const size_t height = frame.rows;
					
				slog::info << "camera input width: " << width  << slog::endl;
				slog::info << "camera input height: " << height  << slog::endl;
				// show live and wait for a key with timeout long enough to show images
				cv::imshow("Live", frame);				
				if (cv::waitKey(1) >= 0)
				{
					break;
				}
			
				//checking inputs from camera
				slog::info << "checking inputs from camera" << slog::endl;
				InputsDataMap inputInfo(network.getInputsInfo());
				if (inputInfo.size() != 1) 
				{
					throw std::logic_error("Checking inputs from camera \n");
				}
				InputInfo::Ptr inputInfoFirst = inputInfo.begin()->second;
				inputInfoFirst->setPrecision(Precision::FP32);
						
			
				//copy data to inputblob
				slog::info << "copy data to inputblob" << slog::endl;
				Blob::Ptr  inputBlob = infer_request.GetBlob(inputInfo.begin()->first);
				matU8ToBlob<float>(frame, inputBlob);
    		}							
		//AI_Toolkit.FST.Openvino.Samual add for opencv real time style transfer 20190305 End   
			
		    // -----------------------------------------------------------------------------------------------------

		    // --------------------------- 7. Do inference ---------------------------------------------------------
		    slog::info << "Start inference (" << FLAGS_ni << " iterations)" << slog::endl;

		    typedef std::chrono::high_resolution_clock Time;
		    typedef std::chrono::duration<double, std::ratio<1, 1000>> ms;
		    typedef std::chrono::duration<float> fsec;

		    double total = 0.0;
		    /** Start inference & calc performance **/
		    for (int iter = 0; iter < FLAGS_ni; ++iter) {
		        auto t0 = Time::now();
		        infer_request.Infer();
		        //std::this_thread::sleep_for(std::chrono::milliseconds(5000));
		        auto t1 = Time::now();
		        fsec fs = t1 - t0;
		        ms d = std::chrono::duration_cast<ms>(fs);
		        total += d.count();
		    }

		    /** Show performance results **/
		    std::cout << std::endl << "Average running time of one iteration: " << total / static_cast<double>(FLAGS_ni)
		              << " ms" << std::endl;

		    if (FLAGS_pc) {
		        printPerformanceCounts(infer_request, std::cout);
		    }
		    // -----------------------------------------------------------------------------------------------------

		    // --------------------------- 8. Process output -------------------------------------------------------
		    const Blob::Ptr output_blob = infer_request.GetBlob(firstOutputName);
		    const auto output_data = output_blob->buffer().as<PrecisionTrait<Precision::FP32>::value_type*>();
		    //const auto output_data = output_blob->buffer().as<PrecisionTrait<Precision::FP16>::value_type*>();

		    size_t num_images = output_blob->getTensorDesc().getDims()[0];
		    size_t num_channels = output_blob->getTensorDesc().getDims()[1];
		    size_t H = output_blob->getTensorDesc().getDims()[2];
		    size_t W = output_blob->getTensorDesc().getDims()[3];
		    size_t nPixels = W * H;
		    		    
		    slog::info << "Output size [N,C,H,W]: " << num_images << ", " << num_channels << ", " << H << ", " << W << slog::endl;
			
		    {
		        std::vector<float> data_img(nPixels * num_channels);

		        for (size_t n = 0; n < num_images; n++) {
		            for (size_t i = 0; i < nPixels; i++) {
		                data_img[i * num_channels] = static_cast<float>(output_data[i + n * nPixels * num_channels] +
		                                                               meanValues[0]);
		                data_img[i * num_channels + 1] = static_cast<float>(
		                        output_data[(i + nPixels) + n * nPixels * num_channels] + meanValues[1]);
		                data_img[i * num_channels + 2] = static_cast<float>(
		                        output_data[(i + 2 * nPixels) + n * nPixels * num_channels] + meanValues[2]);

		                //slog::info << "output_data[i]: " << output_data[i]  << slog::endl;
		                float temp = data_img[i * num_channels];
		                data_img[i * num_channels] = data_img[i * num_channels + 2];
		                data_img[i * num_channels + 2] = temp;

		                if (data_img[i * num_channels] < 0) data_img[i * num_channels] = 0;
		                if (data_img[i * num_channels] > 255) data_img[i * num_channels] = 255;

		                if (data_img[i * num_channels + 1] < 0) data_img[i * num_channels + 1] = 0;
		                if (data_img[i * num_channels + 1] > 255) data_img[i * num_channels + 1] = 255;

		                if (data_img[i * num_channels + 2] < 0) data_img[i * num_channels + 2] = 0;
		                if (data_img[i * num_channels + 2] > 255) data_img[i * num_channels + 2] = 255;
		            }
		            
		            std::vector<unsigned char> data_img2(data_img.begin(), data_img.end());
		            
		            //AI_Toolkit.FST.Openvino.Samual add for opencv real time style transfer 20190305 Start
					if (!FLAGS_i.empty())
					{
				        std::string out_img_name = std::string("out" + std::to_string(n + 1) + ".bmp");
				        std::ofstream outFile;
				        outFile.open(out_img_name.c_str(), std::ios_base::binary);
				        if (!outFile.is_open()) {
				            throw new std::runtime_error("Cannot create " + out_img_name);
				        }
				        writeOutputBmp(data_img2.data(), H, W, outFile);
				        outFile.close();
				        slog::info << "Image " << out_img_name << " created!" << slog::endl;
		            }
		            else
		            {
				        cv::Mat output_buffer(H, W, CV_8UC3, data_img2.data());				        
				        std::ostringstream out;
				        
				        out.str("");
				        out << "Style transfer time: " << std::fixed << std::setprecision(2)
		                << total
		                << " ms ("
		                << 1000.f / total
		                << " fps)";
		            	cv::putText(output_buffer, out.str(), cv::Point2f(0, 45), cv::FONT_HERSHEY_TRIPLEX, 0.5,
		                        cv::Scalar(255, 255, 255));
		                        
				        cv::imshow( "style transfer", output_buffer);
				        
						if (cv::waitKey(1) >= 0)
						{
							break;
						}		            
				        //backup solution
				        //cv::imshow( "output Display window", cv::imread("out1.bmp") );						
					}					
					//AI_Toolkit.FST.Openvino.Samual add for opencv real time style transfer 20190305 End
		        }
		    }
		    if (!FLAGS_i.empty())
		    {
		    	break;
		    }
		    // -----------------------------------------------------------------------------------------------------     
        }
        //AI_Toolkit.FST.Openvino.Samual add for opencv real time style transfer 20190305 End
        
    }
    catch (const std::exception &error) {
        slog::err << error.what() << slog::endl;
        return 1;
    }
    catch (...) {
        slog::err << "Unknown/internal exception happened" << slog::endl;
        return 1;
    }

    slog::info << "Execution successful" << slog::endl;
    return 0;
}
