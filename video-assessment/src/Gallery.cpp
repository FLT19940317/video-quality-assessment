#include "Gallery.h"
#include <opencv2/opencv.hpp>
#include <opencv/highgui.h>
#include <json.hpp>

using namespace cv;

using namespace std::chrono;
using json = nlohmann::json;

Gallery::Gallery() {
	panelWidth = 1;
	panelHeight = 1;
	cout << "New Panel object" << endl;
}

Gallery::Gallery(double width, double height)
{
	if (width < 0 || width > 1 || height < 0 || height > 1) {
		cout << "Width and height percentage value must be between 0 and 1. Set default 1 value" << endl;
		percentageWidth = 1;
		percentageHeight = 1;
	}
	else {
		percentageWidth = width;
		percentageHeight = height;
	}
}

void Gallery::test()
{
	std::cout << "Class test" << endl;
}

void Gallery::setup()
{

	if (!isLocked()) {

		getConfigParams();

		if (!parseOnly) {
			extractVideoData();
		}
		else extractVideoThumbnails();

		parseCsvFeatureVector();
		parseSemanticVector();
		lock();
	}

	if (!loadFiles())
	{
		std::cout << "Load files error" << endl;
		::ofExit(1);
	}
	/*
	The "panel" is a frame. This frame contains the displayed images, and the scroll bar.
	The scroll bar contains a "grip". The user can drag the grip with the mouse.
	*/

	gap = 15;               // Distance between rectangles, and between rectangles and scroll bar
	margin = 50;            // Distance between the edge of the screen and the panel frame
	downMargin = 15;			//Place for buttons
	scrollBarWidth = 20;

	// Now two rectangles, for the scroll bar and his grip placements
	// Coordinates are relative to the panel coordinates, not to the screen coordinates
	// This is a first initialisation, but we don't know many things about these placements at this state
	scrollBarRectangle = ofRectangle(0, 0, scrollBarWidth, 0);
	gripRectangle = ofRectangle(0, 0, scrollBarWidth, 0);

	isDraggingGrip = false; // true when the user is moving the grip
	isMouseOverGrip = false; // true when the mouse is over the grip

// Create images to display
	thumbnailClicked = false;
	thumbnailsWidth = File::thumbnailWidth;
	thumbnailsHeight = File::thumbnailHeight;
	for (int i = 0; i < allFiles.size(); ++i) {
		rectangles.push_back(ofRectangle(0, 0, thumbnailsWidth, thumbnailsHeight));
	}

	Gallery::update();				//To get all coordinates properly

	//Initialize font to display names of files and ratings
	metadataPanel.setup(scrollBarRectangle.x + scrollBarWidth + 1.5*margin, margin + 1);
	name = new ofTrueTypeFont();
	name->load("arial.ttf", 9);
	counter = new ofTrueTypeFont();
	counter->load("arial.ttf", 14);
	playerText = new ofTrueTypeFont();
	playerText->load("arial.ttf", 10);

	ofBackground(127);
	//Initialize filters
	filtersPanel.setup();
	fileSpace = spaceForFileDisplay();		//Calcute avaiable space for file to display
	videoPlay = false;
	cout << "------------------------------------------------------------------" << endl;
}

void Gallery::update()
{
	// The size of the panel. All the screen except margins
	panelWidth = percentageWidth * ofGetWidth() - margin * 2;
	panelHeight = percentageHeight * ofGetHeight() - margin * 2 - downMargin;

	// Space available for displayed rectangles
	availableWidth = panelWidth - scrollBarWidth - gap;

	// Coordinates for first rectangle
	int x = 0;
	int y = 0;

	ofRectangle * r;
	r = new ofRectangle(0, 0, 0, 0);	//Initialize required if no picture is visible

	// Place the rectangles in rows and columns. A row must be smaller than availableWidth
	// After this loop, we know that the rectangles fits the panel width. But the available panel height can be to small to display them all.
	for (int i = 0; i < allFiles.size(); ++i) {	//For all visible files
		if (allFiles[i].getVisible()) {
			r = &rectangles[i];
			r->x = x;
			r->y = y;

			// Now compute the next rectangle position
			x += thumbnailsWidth + gap;
			if (x + thumbnailsWidth > availableWidth) {
				scrollBarRectangle.x = r->getRight() + gap; // Adjust the scroll bar position to draw it just at the right
				x = 0;
				y += thumbnailsHeight + gap;
			}
		}
	}

	gripRectangle.x = scrollBarRectangle.x; // Also adjust the grip x coordinate

	int contentHeight = r->getBottom(); // Total height for all the rectangles
										// TODO: take care if colors.size() == 0

	if (contentHeight > panelHeight) {
		/* In the case where there's not enough room to display all the rectangles */

		/* First, the scroll bar */

		// Show the scroll bar
		isScrollBarVisible = true;
		// Set the scroll bar height to fit the panel height
		scrollBarRectangle.height = panelHeight;


		/* Now, the grip */

		// This ratio is between 0 and 1. The smaller it is, the smaller the grip must be.
		// If its value is 0.5, for example, it means that there's only half of the room in the panel to display all the rectangles.
		float gripSizeRatio = (float)panelHeight / (float)contentHeight;

		// Compute the height of the grip, according to this ratio
		gripRectangle.height = panelHeight * gripSizeRatio;

		/* Now, the vertical scrolling to add to the rectangles position */

		// this ratio, between 0 and 1, tell us the amount of scrolling to add if the grip is at the bottom of the scroll bar
		float scrollMaxRatio = 1 - gripSizeRatio;

		// this ration tell us how much the grip is down. If 0, the grip is at the top of the scroll bar.
		// if 1, the grip is at the bottom of the scroll bar
		float gripYRatio = gripRectangle.y / (scrollBarRectangle.height - gripRectangle.height);

		// Now, the amount of scrolling to do, according to the twos previous ratios
		float contentScrollRatio = gripYRatio * scrollMaxRatio;

		// And now the scrolling value to add to each rectangle y coordinate
		contentScrollY = contentHeight * contentScrollRatio;
	}
	else {
		/* In the case where there's enough room to display all the rectangles */

		isScrollBarVisible = false;
		contentScrollY = 0;
	}
	fileSpace = spaceForFileDisplay();

}

void Gallery::draw()
{

	/* First draw the rectangles */

	// Add a translation to bring the panel to the good position
	ofPushMatrix();
	ofTranslate(margin, margin, 0);

	ofRectangle r;
	ofImage img;
	ofSetColor(255);
	numberOfselectedFiles = 0;


	for (int i = 0; i < allFiles.size(); ++i) {	//For all visible files
		if (allFiles[i].getVisible()) {
			numberOfselectedFiles++;
			r = rectangles[i];				// the rectangle position in the panel
			r.y -= contentScrollY;			// adjust this position according to the scrolling
			img = allFiles[i].thumbnail;    // image to display. OF don't copy big objects like ofImage, so no need to use a pointer.			

			if (r.y < 0) {
				if (r.getBottom() > 0) {
					// Exception 1: If a rectangle is cut at the top of the panel


					if (allFiles[i].getIsCurrentFile()) {

						ofSetColor(255, 255, 0);  // Set the drawing color to yellow							
						ofDrawRectangle(r.x - 3, -3, thumbnailsWidth + 6, thumbnailsHeight + r.y + 16);
						ofSetColor(255);

					}

					ofSetColor(0);
					ofDrawRectangle(r.x, 0, thumbnailsWidth, thumbnailsHeight + r.y + 10); 	// Draw black rectangle
					ofSetColor(255);
					//img.resize(r.width, r.height-gap);

					img.getTextureReference().drawSubsection(r.x, 0, r.width, thumbnailsHeight + r.y - gap, 0, -r.y - gap, r.width, thumbnailsHeight + r.y - gap);
					name->drawString(allFiles[i].name.substr(0, 16), r.x + 3, r.y - 10 + thumbnailsHeight + 0.5*gap);

				}
			}
			else if (r.getBottom() >= panelHeight) {
				if (r.y <= panelHeight) {

					if (allFiles[i].getIsCurrentFile()) {

						ofSetColor(255, 255, 0);  // Set the drawing color to yellow							
						ofDrawRectangle(r.x - 3, r.y - 3, thumbnailsWidth + 6, panelHeight + 6 - r.y);
						ofSetColor(255);

					}
					ofSetColor(0);
					ofDrawRectangle(r.x, r.y, thumbnailsWidth, panelHeight - r.y);
					ofSetColor(255);
					// Exception 2: If a rectangle is cut at the bottom of the panel.				
					img.getTextureReference().drawSubsection(r.x, r.y, r.width, panelHeight - r.y - gap, 0, 0, r.width, panelHeight - r.y - gap);


				}
			}
			else {
				// Draw a rectangle in the panel

				if (allFiles[i].getIsCurrentFile()) {

					ofSetColor(255, 255, 0);  // Set the drawing color to yellow							
					ofDrawRectangle(r.x - 3, r.y - 3, thumbnailsWidth + 6, thumbnailsHeight + 16); 	// Draw white rectangle
					ofSetColor(0);

				}

				ofSetColor(0);  // Set the drawing color to black							
				ofDrawRectangle(r.x, r.y, thumbnailsWidth, thumbnailsHeight + 10); 	// Draw black rectangle
				ofSetColor(255);

				float dif = (thumbnailsHeight - allFiles[i].thumbnail.getHeight()) / 2;
				allFiles[i].thumbnail.draw(r.x, r.y + dif);

				name->drawString(allFiles[i].name.substr(0, 16), r.x + 3, r.y + thumbnailsHeight + 0.5*gap);

			}

			rectangles[i] = r;
		}

		if (i == allFiles.size() - 1)
			counter->drawString(ofToString(numberOfselectedFiles) + " / " + ofToString(allFiles.size()), 2, -7);

	}

	/* Draw the scroll bar, is needed */

	if (isScrollBarVisible) {
		ofSetColor(110);
		ofRect(scrollBarRectangle);
		if (isDraggingGrip || isMouseOverGrip) {
			ofSetColor(230);
		}
		else {
			ofSetColor(180);
		}
		ofRect(gripRectangle);
	}

	// Remove the translation added at the begining
	ofPopMatrix();

	//filtersPanel
	filtersPanel.draw();

	//Draw metadata on when clicked
	if (thumbnailClicked) {
		int metadataPanel_x = scrollBarRectangle.x + scrollBarWidth + 1.5*margin;		//x coordinate of metadata panel 
		int metadataPanel_y = margin + 1;												//y coordinate of metadata panel 
		metadataPanel.draw(metadataPanel_x, metadataPanel_y);

		if (allFiles[choosenFileIndex].getType() == File::fileType::VIDEO)	//Video file choosen
		{
			choosenVideo.draw(fileSpace, videoPlay, playerText);
		}
		else
		{
			cout << "File not initialized" << endl;
		}
	}
}

void Gallery::keyPressed(int key)
{
	//cout << char(key);
	if (std::isspace(key)) {
		videoPlay = !videoPlay;		//Toggle video playing
	}
}

void Gallery::keyReleased(int key)
{
}

void Gallery::mouseMoved(int x, int y)
{
	if (isScrollBarVisible) {
		ofRectangle r = gripRectangle;
		r.translate(margin, margin); // This translation because the coordinates of the grip are relative to the panel,
									 //but the mouse position is relative to the screen
		isMouseOverGrip = r.inside(x, y);
	}
	else {
		isMouseOverGrip = false;
	}

}

void Gallery::mouseDragged(int x, int y, int button)
{
	if (isScrollBarVisible && isDraggingGrip) {

		// Move the grip according to the mouse displacement
		int dy = y - mousePreviousY;
		mousePreviousY = y;
		gripRectangle.y += dy;

		// Check if the grip is still in the scroll bar
		if (gripRectangle.y < 0) {
			gripRectangle.y = 0;
		}
		if (gripRectangle.getBottom() > scrollBarRectangle.getBottom()) {
			gripRectangle.y = scrollBarRectangle.getBottom() - gripRectangle.height;
		}

	}
}

void Gallery::mousePressed(int x, int y, int button) {

	// Check if the click occur on the grip
	if (isScrollBarVisible) {
		ofRectangle r = gripRectangle;
		r.translate(margin, margin); // This translation because the coordinates of the grip are relative to the panel, 
									 //but the mouse position is relative to the screen
		if (r.inside(x, y)) {
			isDraggingGrip = true;
			mousePreviousY = y;
		}
	}

	//Check if thumbnail clicked
	//If we have choose rate or filters, don't check for thumbnail clicking
	if (filtersPanel.isRateClicked(x, y))							//Rate choosen ( tittle or value). Don't change choosenFileIndex or thumbnailClicked Flag now
	{
		cout << "Rate clicked. Index: " << choosenFileIndex << endl;
		if (thumbnailClicked)										//If some file was choosen earlier
		{
			if (filtersPanel.isRateValueClicked(x, y))				//And we want to update rate
			{
				cout << "Index: " << choosenFileIndex << " Rate: " << filtersPanel.getRate() << endl;
				allFiles[choosenFileIndex].rateUpdate(filtersPanel.getRate());	//Update its rate

			}

		}
	}
	else  if (filtersPanel.isFiltersClicked(x, y))					//Filter choosen. Don't change choosenFileIndex or thumbnailClicked flag now
	{
		cout << "Filters clicked. Index: " << choosenFileIndex << endl;
	}
	else  if (filtersPanel.isFiltersMoreClicked(x, y))					//Filter choosen. Don't change choosenFileIndex or thumbnailClicked flag now
	{
		cout << "Filters clicked. Index: " << choosenFileIndex << endl;
	}
	else  if (filtersPanel.isRankingClicked(x, y))					//Ranking choosen. Don't change choosenFileIndex or thumbnailClicked flag now
	{

		cout << "Sort clicked. Index: " << choosenFileIndex << endl;
	}
	else  if (filtersPanel.isSimilarityClicked(x, y))					//Filter choosen. Don't change choosenFileIndex or thumbnailClicked flag now
	{
		cout << "Similarity clicked. Index: " << choosenFileIndex << endl;
	}
	else
	{
		if (checkIfThumbnailClicked(x, y) && (!filtersPanel.isToolbarClicked(x, y)))	//Check if thumbnail clicked. Update choosenFileIndex 
		{
			VideoFile vf = allFiles[choosenFileIndex];

			cout << "-> file: " << vf.name << endl;
			if (vf.semanticValue_1 > 0) {
				cout << vf.semanticID_1 << " - " << clNames[vf.semanticID_1] << " - " <<
					vf.semanticValue_1 * 100 << "%" << endl;
			}
			if (vf.semanticValue_2 > 0) {
				cout << vf.semanticID_2 << " - " << clNames[vf.semanticID_2] << " - " <<
					vf.semanticValue_2 * 100 << "%" << endl;
			}
			if (vf.semanticValue_3 > 0) {
				cout << vf.semanticID_3 << " - " << clNames[vf.semanticID_3] << " - " <<
					vf.semanticValue_3 * 100 << "%" << endl;
			}
			if (vf.semanticValue_4 > 0) {
				cout << vf.semanticID_4 << " - " << clNames[vf.semanticID_4] << " - " <<
					vf.semanticValue_4 * 100 << "%" << endl;
			}
			if (vf.semanticValue_5 > 0) {
				cout << vf.semanticID_5 << " - " << clNames[vf.semanticID_5] << " - " <<
					vf.semanticValue_5 * 100 << "%" << endl;
			}
			cout << "-------------------------------------------------------" << endl;
			if (allFiles[choosenFileIndex].getType() == File::fileType::VIDEO)		//Video file choosen
			{
				videoPlay = false;
				choosenVideo = VideoFile(&allFiles[choosenFileIndex]);

				for (int i = 0; i < allFiles.size(); ++i) {	//For all  files
					allFiles[i].setIsCurrentFile(false);
				}
				allFiles[choosenFileIndex].setIsCurrentFile(true);
				metadataPanel.getData(&choosenVideo);
			}
			else
			{
				cout << "File not initialized" << endl;
			}
		}
		//else {}
	}
}

void Gallery::mouseReleased(int x, int y, int button) {
	isDraggingGrip = false;

	if (filtersPanel.isFiltersClicked(x, y) || filtersPanel.isFiltersMoreClicked(x, y) || filtersPanel.isRankingClicked(x, y)
		|| filtersPanel.isSimilarityClicked(x, y))
	{
		high_resolution_clock::time_point t1 = high_resolution_clock::now(); // note time before execution
		filtersPanel.filter(&allFiles[0], allFiles.size(), choosenFileIndex);
		high_resolution_clock::time_point t2 = high_resolution_clock::now(); // note time after execution
		std::cout << "operation duration: "
			<< std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count()
			<< " ms\n";
	}

}

void Gallery::windowResized(int w, int h) {

	// Place the grip at the top of the scroll bar if the size of the panel change
	gripRectangle.y = 0;
	fileSpace = spaceForFileDisplay();		//Calcute avaiable space for file to display
}

void Gallery::dragEvent(ofDragInfo dragInfo) {}

void Gallery::gotMessage(ofMessage msg) {}

bool Gallery::isLocked() {
	ofFile fileToRead(ofToDataPath(xmlFolderPath + "locked.txt")); // a file 
	if (fileToRead.exists()) {
		std::cout << "Extraction process locked!" << endl;
		return true;
	}
	else {
		std::cout << "Extraction process unlocked!" << endl;
		return false;
	}
}
void Gallery::lock() {
	ofFile newFile(ofToDataPath(xmlFolderPath + "locked.txt"));
	newFile.create();
}

void Gallery::unLock() {
	ofFile deleteFile(ofToDataPath(xmlFolderPath + "locked.txt"));
	deleteFile.remove();
}

bool Gallery::extractVideoThumbnails() {
	cout << " [*] Extracting video thumbnails!" << endl;

	size_t nFiles;                              //number of files to process
	glob(inputFolder.c_str(), fileNames);    // loads path to filenames into vector
	nFiles = fileNames.size();
	cout << " [!] Thumbnails to process: " << nFiles << "\n\n";

	int nv = 0;      // index to transverse video collection
	Mat frame;       //matrix to save each frame pixel data

	while (nv < nFiles)
	{
		VideoCapture cap;

		string filePath = fileNames.at(nv);
		try {
			cap.open(filePath.c_str());

			cap >> frame;
			double tempHeigth;
			if (frame.rows > frame.cols) {
				tempHeigth = (thumbnailWidth / frame.rows*frame.cols);
			}
			else { tempHeigth = thumbnailHeight; }

			Mat thumbnailImage;
			resize(frame, thumbnailImage, Size(thumbnailWidth, tempHeigth), 0, 0, INTER_NEAREST);
			size_t lastindex1 = filePath.find_last_of("\\");
			string name = filePath.substr(lastindex1);
			size_t lastindex2 = name.find_last_of(".");
			name = name.substr(0, lastindex2);
			string path = thumbnailFolderPath + name + ".jpg";
			imwrite(path, thumbnailImage);
		}
		catch (Exception &e) {
			const char *err_msg = e.what();
			cout << "exception!: " << err_msg << std::endl;
		}
		cap.release();
		nv++;
	}
	return true;
}
bool Gallery::extractVideoData() {


	cout << " [*] Cognitus visual feature extraction module" << endl;
	cout << " [*] Video input folder: " << inputFolder << endl;

	mlc.init(); //initialize machine learning module instance
	ex.init(); //initialize feature extraction module instance
	//cout << "testing " << mlc.predictTestSample(19) << endl;

	//lets load all filenames
	size_t nFiles;                    //number of files to process
	glob(inputFolder.c_str(), fileNames); // loads path to filenames into vector
	nFiles = fileNames.size();
	cout << " [!] number of files to process: " << nFiles << "\n\n";

	//Save metadata to the output csv file
	ofstream myfile(dataOutputPath.c_str());
	//and semantic data too
	ofstream mysemanticfile(semanticDataOutputPath.c_str());

	///main loop
	int nv = 0;
	int aesthetic = 0;
	int interest = 0;
	while (nv < nFiles)
	{
		auto start = chrono::high_resolution_clock::now();
		ex.extractFromVideo(fileNames.at(nv), nv + 1);

		vector < pair <double, int > > semanticTemp;

		semanticTemp = ex.getSemanticMap();

		if (mysemanticfile.is_open()) {

			int x = 0;

			while (x < semanticTemp.size()) {

				if (x != semanticTemp.size() - 1) {

					mysemanticfile << semanticTemp.at(x).second << ","
						<< semanticTemp.at(x).first << ",";

				}
				else {
					mysemanticfile << semanticTemp.at(x).second << ","
						<< semanticTemp.at(x).first;
				}
				x++;

			}

			mysemanticfile << "\n";
			mysemanticfile.flush();

		}
		json jsonA = ex.getJsonA();
		json jsonI = ex.getJsonI();

		aesthetic = mlc.predictSample(jsonA, 0);

		interest = mlc.predictSample(jsonI, 1);
		//interest = 3;
		

		json jsonSample = ex.getJsonAll();

		string finalName;
		if (myfile.is_open()) {
			string tempName = fileNames.at(nv);
			string destName = tempName.substr(tempName.find_last_of('/') + 1, tempName.size());
			finalName = destName.substr(6, destName.find_last_of('.'));

			myfile << nv + 1
				<< "," << jsonSample["width"]
				<< "," << jsonSample["height"]
				<< "," << jsonSample["red_ratio"]
				<< "," << jsonSample["red_moments1"]
				<< "," << jsonSample["red_moments2"]
				<< "," << jsonSample["green_ratio"]
				<< "," << jsonSample["green_moments1"]
				<< "," << jsonSample["green_moments2"]
				<< "," << jsonSample["blue_ratio"]
				<< "," << jsonSample["blue_moments1"]
				<< "," << jsonSample["blue_moments2"];

			myfile << "," << jsonSample["focus"]
				<< "," << jsonSample["luminance"]
				<< "," << jsonSample["luminance_std"]
				<< "," << jsonSample["red_moments3"]
				<< "," << jsonSample["red_moments4"]
				<< "," << jsonSample["green_moments3"]
				<< "," << jsonSample["green_moments4"]
				<< "," << jsonSample["blue_moments3"]
				<< "," << jsonSample["blue_moments4"];

			myfile << "," << jsonSample["dif_hues"]
				<< "," << jsonSample["faces"]
				<< "," << jsonSample["faces_area"]
				<< "," << jsonSample["smiles"]
				<< "," << jsonSample["rule_of_thirds"]
				<< "," << jsonSample["static_saliency"]
				<< "," << jsonSample["rank_sum"]
				<< "," << jsonSample["fps"]
				<< "," << jsonSample["hues_std"]
				<< "," << jsonSample["shackiness"]
				<< "," << jsonSample["motion_mag"]
				<< "," << jsonSample["fg_area"]
				<< "," << jsonSample["shadow_area"]
				<< "," << jsonSample["bg_area"]
				<< "," << jsonSample["camera_move"]
				<< "," << jsonSample["focus_diff"]
				<< "," << aesthetic
				<< "," << interest
				<< "," << jsonSample["hues_skewness"]
				<< "," << jsonSample["hues_kurtosis"];


			myfile << "," << jsonSample["eh_0"]
				<< "," << jsonSample["eh_1"]
				<< "," << jsonSample["eh_2"]
				<< "," << jsonSample["eh_3"]
				<< "," << jsonSample["eh_4"]
				<< "," << jsonSample["eh_5"]
				<< "," << jsonSample["eh_6"]
				<< "," << jsonSample["eh_7"]
				<< "," << jsonSample["eh_8"]
				<< "," << jsonSample["eh_9"]
				<< "," << jsonSample["eh_10"]
				<< "," << jsonSample["eh_11"]
				<< "," << jsonSample["eh_12"]
				<< "," << jsonSample["eh_13"]
				<< "," << jsonSample["eh_14"]
				<< "," << jsonSample["eh_15"]
				<< "," << jsonSample["eh_16"]
				<< "," << jsonSample["entropy"]
				<< "," << jsonSample["edge_strenght"];

			myfile << "," << jsonSample["luminance_skewness"]
				<< "," << jsonSample["luminance_kurtosis"]
				<< "," << jsonSample["entropy_std"]
				<< "," << jsonSample["entropy_skewness"]
				<< "," << jsonSample["entropy_kurtosis"]
				<< "," << jsonSample["focus_std"]
				<< "," << jsonSample["focus_skewness"]
				<< "," << jsonSample["focus_kurtosis"]
				<< "," << jsonSample["mag_std"]
				<< "," << jsonSample["mag_skewness"]
				<< "," << jsonSample["mag_kurtosis"]
				<< "," << jsonSample["uflowx_mean"]
				<< "," << jsonSample["uflowx_std"]
				<< "," << jsonSample["uflowx_skewness"]
				<< "," << jsonSample["uflowx_kurtosis"]
				<< "," << jsonSample["uflowy_mean"]
				<< "," << jsonSample["uflowy_std"]
				<< "," << jsonSample["uflowy_skewness"]
				<< "," << jsonSample["uflowy_kurtosis"];

			myfile << "," << jsonSample["sflowx_mean"]
				<< "," << jsonSample["sflowx_std"]
				<< "," << jsonSample["sflowx_skewness"]
				<< "," << jsonSample["sflowx_kurtosis"]
				<< "," << jsonSample["sflowy_mean"]
				<< "," << jsonSample["sflowy_std"]
				<< "," << jsonSample["sflowy_skewness"]
				<< "," << jsonSample["sflowy_kurtosis"]
				<< "," << jsonSample["colorfullness_rg1"]
				<< "," << jsonSample["colorfullness_rg2"]
				<< "," << jsonSample["colorfullness_yb1"]
				<< "," << jsonSample["colorfullness_yb2"]
				<< "," << jsonSample["duration"]
				<< "," << jsonSample["saturation_1"]
				<< "," << jsonSample["saturation_2"]
				<< "," << jsonSample["brightness_1"]
				<< "," << jsonSample["brightness_2"]
				<< "," << jsonSample["colorfull_1"]
				<< "," << jsonSample["colorfull_2"];

			myfile << "\n";
			myfile.flush();

		}
		auto end = chrono::high_resolution_clock::now();
		cout << " [!] File " << finalName << " processed in : " <<
			duration_cast<chrono::milliseconds>(end - start).count() << " ms\n" << endl;
		nv++;


	}
	mysemanticfile.close();
	myfile.close();
	return true;
}

bool Gallery::loadFiles()
{
	size_t vectorSize = 0;							//Size of vector with thumbnails and their names

	ofDirectory vidDir(VideoFile::filesFolderPath);		//Folder with videos
	for (string s : VideoFile::availableFormats)
	{
		vidDir.allowExt(s);								//dir will read only particular formats
	}
	vidDir.listDir(VideoFile::filesFolderPath);

	if (vidDir.size() > 0)								//If there are some videos
		vectorSize += vidDir.size();					//Increase vector size

	if (vectorSize > 0)									//If there are some files
	{
		allFiles.assign(vectorSize, VideoFile());			//Create vector of size equal to number of images and videos
		clNames.assign(1000, "");
		clNames = readClassNames();

		for (int k = 0; k < vectorSize; ++k)
		{

			VideoFile tmpVideo = VideoFile(vidDir.getName(k), vidDir.getPath(k));

			if (ofFile::doesFileExist(tmpVideo.xmlPath)) {			//If doesn't xmlDataExists

				tmpVideo.getMetadataFromXml();		//Get metada from the xml
			}
			else {

				vector<string> test = getIndividualSample(to_string(k + 1));
				vector<pair<double, int> > semanticSample = semanticData[k];
				tmpVideo.getMetadataFromCsv(test);
				tmpVideo.getMetadataFromSemanticSample(semanticSample);
				tmpVideo.generateXmlFile();
				tmpVideo.getMetadataFromXml();
			}

			allFiles[k] = tmpVideo;								//Create file with initialized data
			allFiles[k].setType(File::fileType::VIDEO);

		}
		//cout << "Feature vector loaded!" << endl;
		return true;

	}
	return false;

}

//Check if thumbnail was clicked. Sets thumbnailClicked flag and clicked thumbnail position
bool Gallery::checkIfThumbnailClicked(int x, int y)
{
	//Don't change anything if grip was clicked
	if (isMouseOverGrip) {
		return false;
	}

	thumbnailClicked = false;					//Assume that we didn't clicked thumbnail
	for (int i = 0; i < allFiles.size(); ++i) {	//For all visible files
		if (allFiles[i].getVisible()) {
			ofPoint mousePoint;
			mousePoint.set(x - margin, y - margin);		//Coordinates of mouse inside panel area
			if (rectangles[i].inside(mousePoint)) {		//If mouse is over some image
				choosenFileIndex = i;				//Save index
				thumbnailClicked = true;
				return thumbnailClicked;				//Return true
			}
		}
	}
	choosenFileIndex = -1;							//No thumbnail clicked (nor toolbar nor grip)
	return thumbnailClicked;							//If thumbnail not clicked
}

bool Gallery::toolBarClicked(int x, int y)
{
	return filtersPanel.isToolbarClicked(x, y);
}

ofRectangle Gallery::spaceForFileDisplay()
{
	int x = metadataPanel.getCordX();
	int y = metadataPanel.getHeight() + 1.5 * margin;
	ofRectangle space = ofRectangle(x, y, 320, 240);

	return space;
}

bool Gallery::parseSemanticVector() {

	semanticData.assign(totalFiles, vector<pair<double, int>>(5));
	ifstream myReadFile;
	myReadFile.open(semanticDataOutputPath.c_str());

	string line;
	if (myReadFile.fail())
	{
		cout << "fail loading semantic vector!" << endl;
		myReadFile.close();
		return false;
	}
	else {

		vector<pair<double, int>> tempVector;
		tempVector.assign(5, pair<double, int>(0.0, 0));
		int l = 0;
		while (getline(myReadFile, line)) {

			stringstream temp(line);
			string word;
			int w = 0;
			int par = 0;

			while (getline(temp, word, ',')) {

				if (w % 2 == 0 || w == 0) {

					tempVector.at(par).second = std::stoi(word);
				}
				else {
					tempVector.at(par).first = std::stod(word);
					par++;

				}
				w++;
			}
			int dif = 5 - w / 2;
			while (dif > 0) {
				tempVector.at(5 - dif).first = -1;
				tempVector.at(5 - dif).second = -1;

				dif--;
			}
			semanticData.at(l) = tempVector;
			l++;
		}

	}
	myReadFile.close();
	return true;
}


bool Gallery::parseCsvFeatureVector() {

	ifstream myReadFile;
	myReadFile.open(dataOutputPath.c_str());

	string line;

	if (myReadFile.fail())
	{
		cout << "fail loading csv feature vector!" << endl;
		myReadFile.close();
		return false;
	}
	else {

		int l = 0;
		int w = 0;

		csvData.assign(totalFiles, vector<string>(100, ""));

		while (getline(myReadFile, line)) {

			stringstream temp(line);
			string word;
			vector <string> linevec;
			linevec.assign(100, "");

			while (getline(temp, word, ',')) {

				linevec[w] = word;
				w++;
			}

			w = 0;
			csvData[l] = linevec;
			l++;
		}

	}
	myReadFile.close();
	return true;
}

vector<string> Gallery::getIndividualSample(string name) {

	if (csvData.size() > 0)
	{
		int i = 0;
		while (i < csvData.size())
		{
			if (csvData[i][0] == name) return csvData[i];
			i++;
		}
	}
	else {
		vector<string> vecStr;
		vecStr.assign(1, "");
		return vecStr;
	}
}

std::vector<String> Gallery::readClassNames()
{
	const char *filename = "../bin/data/dnn/synset_words.txt";
	std::vector<String> cNames;
	std::ifstream fp(filename);
	if (!fp.is_open())
	{
		std::cerr << "File with classes labels not found: " << filename << std::endl;
		exit(-1);
	}
	std::string name;
	while (!fp.eof())
	{
		std::getline(fp, name);
		if (name.length())
			cNames.push_back(name.substr(name.find(' ') + 1));
	}
	//cout << filename << " loaded!" << endl;
	fp.close();
	return cNames;
}

void Gallery::getConfigParams() {

	ofXml* xml = new ofXml(ex.configPath);
	if (xml->load(ex.configPath)) {
		parseOnly = xml->getValue<bool>("//PARSE_ONLY");
		inputFolder = xml->getValue<string>("//INPUT_FOLDER");
		totalFiles = xml->getValue<int>("//TOTAL_FILES");
	}
}
string Gallery::thumbnailFolderPath = "../bin/data/thumbnails/videos/";