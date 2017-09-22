#ifndef _OS_METRIC_H
#define _OS_METRIC_H
namespace OS {
	enum MetricType {
		MetricType_String,
		MetricType_Integer,
		MetricType_Float,
		MetricType_UnixTime,
		MetricType_Array,
	};

	typedef struct _ScalarValue {
		std::string _str;
		long long   _int;
		float 		_float;
	} MetricScalarValue;

	typedef struct _Value;
	typedef struct _ArrayValue {
		std::vector<std::pair<MetricType, struct _Value> > values;
	} MetricArrayValue;


	typedef struct _Value {
		std::string key;
		struct _ScalarValue value;
		struct _ArrayValue arr_value;
		MetricType type;
	} MetricValue;

	typedef struct _MetricInstance {
		std::string key;
		MetricValue value;
	} MetricInstance;
};
#endif // _OS_METRIC_H