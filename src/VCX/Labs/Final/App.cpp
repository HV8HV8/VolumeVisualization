#include "App.h"
#include "Engine/app.h"
#include "Engine/prelude.hpp"

using namespace VCX::Engine;
using namespace VCX::Labs::Final;

App::App() {
}

void App::OnFrame() {
    if (_currentAlgorithm == Algorithm::RayCasting) {
        _caseRayCasting.OnFrame(*this);
    } else if (_currentAlgorithm == Algorithm::SliceViewer) {
        _caseSliceViewer.OnFrame(*this);
    } else if (_currentAlgorithm == Algorithm::Splatting) {
        _caseSplatting.OnFrame(*this);
    } else {
        _caseSliceCompositing.OnFrame(*this); 
    }
}