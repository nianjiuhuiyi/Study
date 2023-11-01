//#include <stdio.h>
//#include <gst/gst.h>
//
//int func1(int argc, char *argv[]) {
//	GstElement *pipeline;
//	GstBus *bus;
//	GstMessage *msg;
//
//	/* Initialize GStreamer */
//	gst_init(&argc, &argv);
//
//	/* Build the pipeline */
//	//pipeline = gst_parse_launch("playbin uri=https://www.freedesktop.org/software/gstreamer-sdk/data/media/sintel_trailer-480p.webm", NULL);
//	 pipeline = gst_parse_launch("playbin uri=rtsp://192.168.108.11:554/user=admin&password=&channel=1&stream=1.sdp?", NULL);
//	
//	// 注意是file:/// 是有三条斜杠
//	//pipeline = gst_parse_launch("playbin uri=file:///C:/Users/Administrator/Videos/sintel_trailer-480p.webm", NULL);
//
//	/* Start playing */
//	gst_element_set_state(pipeline, GST_STATE_PLAYING);
//
//	/* Wait until error or EOS */
//	bus = gst_element_get_bus(pipeline);
//	msg =
//		gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
//			GST_MESSAGE_ERROR | GST_MESSAGE_EOS);
//	//说明： 这里c中可以这么写，会自动转换：GST_MESSAGE_ERROR | GST_MESSAGE_EOS
//	// c++中必须手动转换类型，static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS)，， 
//
//
//	/* See next tutorial for proper error message handling/parsing */
//	if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) { 
//		g_error("An error occurred! Re-run with the GST_DEBUG=*:WARN environment "
//			"variable set for more details.");
//	}
//
//	/* Free resources */
//	gst_message_unref(msg);
//	gst_object_unref(bus);
//	gst_element_set_state(pipeline, GST_STATE_NULL);
//	gst_object_unref(pipeline);
//	return 0;
//}
//
//// demo2
//int func2(int argc, char *argv[]) {
//	printf("hello world! \n");
//	GstElement *pipeline, *source, *sink;
//	GstBus *bus;
//	GstMessage *msg;
//	GstStateChangeReturn ret;
//
//	/* Initialize GStreamer */
//	gst_init(&argc, &argv);
//
//	/* Create the elements */
//	source = gst_element_factory_make("videotestsrc", "source");
//	sink = gst_element_factory_make("autovideosink", "sink");
//
//	/* Create the empty pipeline */
//	pipeline = gst_pipeline_new("test-pipeline");
//
//	if (!pipeline || !source || !sink) {
//		g_printerr("Not all elements could be created.\n");
//		return -1;
//	}
//
//	/* Build the pipeline */
//	gst_bin_add_many(GST_BIN(pipeline), source, sink, NULL);
//	if (gst_element_link(source, sink) != TRUE) {
//		g_printerr("Elements could not be linked.\n");
//		gst_object_unref(pipeline);
//		return -1;
//	}
//
//	/* Modify the source's properties */
//	g_object_set(source, "pattern", 0, NULL);
//
//	/* Start playing */
//	ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
//	if (ret == GST_STATE_CHANGE_FAILURE) {
//		g_printerr("Unable to set the pipeline to the playing state.\n");
//		gst_object_unref(pipeline);
//		return -1;
//	}
//
//	/* Wait until error or EOS */
//	bus = gst_element_get_bus(pipeline);
//	msg =
//		gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
//			GST_MESSAGE_ERROR | GST_MESSAGE_EOS);
//
//	/* Parse message */
//	if (msg != NULL) {
//		GError *err;
//		gchar *debug_info;
//
//		switch (GST_MESSAGE_TYPE(msg)) {
//		case GST_MESSAGE_ERROR:
//			gst_message_parse_error(msg, &err, &debug_info);
//			g_printerr("Error received from element %s: %s\n",
//				GST_OBJECT_NAME(msg->src), err->message);
//			g_printerr("Debugging information: %s\n",
//				debug_info ? debug_info : "none");
//			g_clear_error(&err);
//			g_free(debug_info);
//			break;
//		case GST_MESSAGE_EOS:
//			g_print("End-Of-Stream reached.\n");
//			break;
//		default:
//			/* We should not reach here because we only asked for ERRORs and EOS */
//			g_printerr("Unexpected message received.\n");
//			break;
//		}
//		gst_message_unref(msg);
//	}
//
//	/* Free resources */
//	gst_object_unref(bus);
//	gst_element_set_state(pipeline, GST_STATE_NULL);
//	gst_object_unref(pipeline);
//	return 0;
//}
//
//// demo3
///* Structure to contain all our information, so we can pass it to callbacks */
//typedef struct _CustomData {
//	GstElement *pipeline;
//	GstElement *source;
//	GstElement *convert;
//	GstElement *resample;
//	GstElement *sink;
//} CustomData;
//
///* Handler for the pad-added signal */
//static void pad_added_handler(GstElement *src, GstPad *pad, CustomData *data);
//
//int func3(int argc, char *argv[]) {
//	CustomData data;
//	GstBus *bus;
//	GstMessage *msg;
//	GstStateChangeReturn ret;
//	gboolean terminate = FALSE;
//
//	/* Initialize GStreamer */
//	gst_init(&argc, &argv);
//
//	/* Create the elements */
//	data.source = gst_element_factory_make("uridecodebin", "source");
//	data.convert = gst_element_factory_make("audioconvert", "convert");
//	data.resample = gst_element_factory_make("audioresample", "resample");
//	data.sink = gst_element_factory_make("autoaudiosink", "sink");
//
//	/* Create the empty pipeline */
//	data.pipeline = gst_pipeline_new("test-pipeline");
//
//	if (!data.pipeline || !data.source || !data.convert || !data.resample || !data.sink) {
//		g_printerr("Not all elements could be created.\n");
//		return -1;
//	}
//
//	/* Build the pipeline. Note that we are NOT linking the source at this
//	 * point. We will do it later. */
//	gst_bin_add_many(GST_BIN(data.pipeline), data.source, data.convert, data.resample, data.sink, NULL);
//	if (!gst_element_link_many(data.convert, data.resample, data.sink, NULL)) {
//		g_printerr("Elements could not be linked.\n");
//		gst_object_unref(data.pipeline);
//		return -1;
//	}
//
//	/* Set the URI to play */
//	g_object_set(data.source, "uri", "https://vd4.bdstatic.com/mda-mk84w7sja77nzw6m/sc/cae_h264_clips/1636428765604680195/mda-mk84w7sja77nzw6m.mp4?auth_key=1636526856-0-0-87055c23af609a97a6f4f98ef887fe7d&bcevod_channel=searchbox_feed&pd=1&pt=3&abtest=", NULL);
//
//	/* Connect to the pad-added signal */
//	g_signal_connect(data.source, "pad-added", G_CALLBACK(pad_added_handler), &data);
//
//	/* Start playing */
//	ret = gst_element_set_state(data.pipeline, GST_STATE_PLAYING);
//	if (ret == GST_STATE_CHANGE_FAILURE) {
//		g_printerr("Unable to set the pipeline to the playing state.\n");
//		gst_object_unref(data.pipeline);
//		return -1;
//	}
//
//	/* Listen to the bus */
//	bus = gst_element_get_bus(data.pipeline);
//	do {
//		msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
//			GST_MESSAGE_STATE_CHANGED | GST_MESSAGE_ERROR | GST_MESSAGE_EOS);
//
//		/* Parse message */
//		if (msg != NULL) {
//			GError *err;
//			gchar *debug_info;
//
//			switch (GST_MESSAGE_TYPE(msg)) {
//			case GST_MESSAGE_ERROR:
//				gst_message_parse_error(msg, &err, &debug_info);
//				g_printerr("Error received from element %s: %s\n", GST_OBJECT_NAME(msg->src), err->message);
//				g_printerr("Debugging information: %s\n", debug_info ? debug_info : "none");
//				g_clear_error(&err);
//				g_free(debug_info);
//				terminate = TRUE;
//				break;
//			case GST_MESSAGE_EOS:
//				g_print("End-Of-Stream reached.\n");
//				terminate = TRUE;
//				break;
//			case GST_MESSAGE_STATE_CHANGED:
//				/* We are only interested in state-changed messages from the pipeline */
//				if (GST_MESSAGE_SRC(msg) == GST_OBJECT(data.pipeline)) {
//					GstState old_state, new_state, pending_state;
//					gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
//					g_print("Pipeline state changed from %s to %s:\n",
//						gst_element_state_get_name(old_state), gst_element_state_get_name(new_state));
//				}
//				break;
//			default:
//				/* We should not reach here */
//				g_printerr("Unexpected message received.\n");
//				break;
//			}
//			gst_message_unref(msg);
//		}
//	} while (!terminate);
//
//	/* Free resources */
//	gst_object_unref(bus);
//	gst_element_set_state(data.pipeline, GST_STATE_NULL);
//	gst_object_unref(data.pipeline);
//	return 0;
//}
//
///* This function will be called by the pad-added signal */
//static void pad_added_handler(GstElement *src, GstPad *new_pad, CustomData *data) {
//	GstPad *sink_pad = gst_element_get_static_pad(data->convert, "sink");
//	GstPadLinkReturn ret;
//	GstCaps *new_pad_caps = NULL;
//	GstStructure *new_pad_struct = NULL;
//	const gchar *new_pad_type = NULL;
//
//	g_print("Received new pad '%s' from '%s':\n", GST_PAD_NAME(new_pad), GST_ELEMENT_NAME(src));
//
//	/* If our converter is already linked, we have nothing to do here */
//	if (gst_pad_is_linked(sink_pad)) {
//		g_print("We are already linked. Ignoring.\n");
//		goto exit;
//	}
//
//	/* Check the new pad's type */
//	new_pad_caps = gst_pad_get_current_caps(new_pad);
//	new_pad_struct = gst_caps_get_structure(new_pad_caps, 0);
//	new_pad_type = gst_structure_get_name(new_pad_struct);
//	if (!g_str_has_prefix(new_pad_type, "audio/x-raw")) {
//		g_print("It has type '%s' which is not raw audio. Ignoring.\n", new_pad_type);
//		goto exit;
//	}
//
//	/* Attempt the link */
//	ret = gst_pad_link(new_pad, sink_pad);
//	if (GST_PAD_LINK_FAILED(ret)) {
//		g_print("Type is '%s' but link failed.\n", new_pad_type);
//	}
//	else {
//		g_print("Link succeeded (type '%s').\n", new_pad_type);
//	}
//
//exit:
//	/* Unreference the new pad's caps, if we got them */
//	if (new_pad_caps != NULL)
//		gst_caps_unref(new_pad_caps);
//
//	/* Unreference the sink pad */
//	gst_object_unref(sink_pad);
//}
//
//
//
//
//
//int main(int argc, char*argv[]) {
//	func1(argc, argv);   // demo1   // rtsp
//	//func2(argc, argv);   // demo2（自带的）
//	//func3(argc, argv);   // demo3(就是只播放网络视频的声音)
//	return 0;
//}