#include <iostream>
#include <vector>
#include <string>
#include <PluginManager.h>
#include <Player.h>
#include <PlayList.h>
#include <MediaLoader.h>
#include <scx/Thread.hpp>
using namespace std;
using namespace scx;
using namespace mous;

Player* gPlayer = NULL;

void OnFinished()
{
    cout << "Finished!" << endl;
}

void OnPlaying()
{
    while (true) {
	if (gPlayer == NULL)
	    break;
	uint64_t ms = gPlayer->GetCurrentMs();
	cout << gPlayer->GetBitRate() << " kbps " <<
	    ms/1000/60 << ":" << ms/1000%60 << "." << ms%1000 << '\r' << flush;
	usleep(200*1000);
    }
}

int main(int argc, char** argv)
{
    bool paused = false;

    PluginManager mgr;
    mgr.LoadPluginDir("./plugins");

    /**
     * Dump all plugin path.
     */
    vector<string> pathList;
    mgr.GetPluginPath(pathList);
    for (size_t i = 0; i < pathList.size(); ++i) {
	cout << ">> " << pathList[i] << endl;
	const PluginInfo* info = mgr.GetPluginInfo(pathList[i]);
	cout << ">>>> " << info->author << endl;
	cout << ">>>> " << info->name << endl;
	cout << ">>>> " << info->description << endl;
	cout << ">>>> " << info->version << endl;
    }
    cout << endl;

    /**
     * Get all plugin instances.
     */
    vector<PluginAgent*> decoderAgentList;
    mgr.GetPluginAgents(decoderAgentList, PluginType::Decoder);
    cout << ">> decoders count:" << decoderAgentList.size() << endl;

    vector<PluginAgent*> rendererAgentList;
    mgr.GetPluginAgents(rendererAgentList, PluginType::Renderer);
    cout << ">> renderers count:" << rendererAgentList.size() << endl;
    cout << endl;

    /**
     * Check args enough.
     */
    if (argc < 2) {
	cout << "Usage:" << endl;
	cout << "mous-cli [-r] [file]" << endl;
	cout << "-r\tRepeat mode." << endl;
	return -1;
    }

    /**
     * Check plugins enough.
     */
    if (decoderAgentList.empty() || rendererAgentList.empty())
	return -2;

    /**
     * Setup loader.
     */
    MediaLoader loader;

    /**
     * Setup player.
     */
    Player player;
    player.SetRendererDevice("/dev/dsp");
    player.SigFinished.Connect(&OnFinished);
    player.RegisterPluginAgent(rendererAgentList[0]);
    for (size_t i = 0; i < decoderAgentList.size(); ++i) {
	player.RegisterPluginAgent(decoderAgentList[i]);
    }

    /**
     * Begin to play.
     */
    player.Open(argv[1]);
    player.Play();
    Thread th;
    gPlayer = &player;
    th.Run(Function<void (void)>(&OnPlaying));

    char ch = ' ';
    while (ch != 'q') {
	cin >> ch;
	switch (ch) {
	    case 'q':
		player.Stop();
		player.Close();
		break;

	    case 's':
		paused = false;
		player.Stop();
		break;

	    case 'p':
		if (paused) {
		    player.Resume();
		    paused = false;
		} else {
		    player.Pause();
		    paused = true;
		}
		break;

	    case 'r':
		player.Play();
		break;
	}
    }

    gPlayer = NULL;
    th.Join();

    player.UnregisterAll();
    mgr.UnloadAllPlugins();

    return 0;
}
