#pragma once

/*

    - Create~ 로 만든 객체는 락 잡고 생성 소멸하므로, 루프마다 사용하지 말 것.
    - 같은 이름·속성으로 다시 생성하면 같은 객체가 나옴
    - Init에서 설정한 exporter가 별도 스레드에서 OTEL에 내보냄
    - NETLIB_ENABLE_METRICS : 켜면 측정 시작. 끄면 측정 x, 성능에 영향 x.
    - NETLIB_ENABLE_OTEL : otel exporter  사용 가능. 안 켜도 콘솔 출력이 가능
 */

#include "MetricHandles.h"
#include "MetricHistogram.h"
#include "MetricRegistry.h"
