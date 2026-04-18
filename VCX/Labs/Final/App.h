#pragma once

#include "Engine/app.h"
#include "CaseRayCasting.h"
#include "CaseSliceViewer.h"
#include "CaseSplatting.h"
#include "CaseSliceCompositing.h"

namespace VCX::Labs::Final {

class App : public VCX::Engine::IApp {
public:
    App();
    void OnFrame() override;
    
    // 渲染算法枚举
    enum class Algorithm {
        RayCasting = 0,        // 光线投射
        SliceViewer = 1,       // 切片查看器
        Splatting = 2,         // 体素散射
        SliceCompositing = 3   // 硬件加速切片合成
    };
    
    Algorithm GetCurrentAlgorithm() const { return _currentAlgorithm; }
    void SetCurrentAlgorithm(Algorithm algo) { _currentAlgorithm = algo; }
    
private:
    CaseRayCasting _caseRayCasting;             // 光线投射实例
    CaseSliceViewer _caseSliceViewer;           // 切片查看器实例
    CaseSplatting _caseSplatting;               // 体素散射实例
    CaseSliceCompositing _caseSliceCompositing; // 切片合成实例
    Algorithm _currentAlgorithm{Algorithm::RayCasting}; 
};

} // namespace VCX::Labs::Final